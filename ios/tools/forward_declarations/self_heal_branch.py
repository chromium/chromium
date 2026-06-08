# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compiler-driven automated self-healing loop for forward declarations.

Compiles specified targets iteratively, parses Clang diagnostics for missing headers,
resolves optimal import declarations, and commits patches automatically.
"""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import time

# Dynamic sys.path configuration based on script location
script_dir = Path(__file__).resolve().parent
sys.path.append(str(script_dir))
import forward_declarations_lib
import heal_gn_check


def run_cmd(args):
    """Runs a shell command synchronously and returns the result."""
    result = subprocess.run(args, capture_output=True, text=True)
    return result


_SYMBOL_CACHE = {}


def find_header_for_type(src_root, type_name):
    """Scans repository to find the header defining a C++ or ObjC type.

    Optimized lookup strategy:
    1. Checks in-memory session cache to return prior mapped symbols.
    2. Interrogates core mappings dictionary for standard baseline types.
    3. Fast path: Executes native git grep against codebase index to
       locate definitions in milliseconds.
    4. Fallback path: Recursively walks primary directory headers.
    """
    # 1. Check cache to avoid redundant file system queries
    if type_name in _SYMBOL_CACHE:
        return _SYMBOL_CACHE[type_name]

    # 2. Check standard system mappings ledger
    for mapped_type, path in forward_declarations_lib.CORE_HEADER_MAPPING.items(
    ):
        if type_name == mapped_type or type_name.endswith("::" + mapped_type):
            _SYMBOL_CACHE[type_name] = path
            return path

    clean_type = type_name.split("::")[-1]
    # Precise matching syntax patterns guaranteeing candidate header
    # definitions (classes, protocols, structs)
    # 1. Matches Objective-C @interface definitions (e.g. '@interface MyClass')
    # 2. Matches Objective-C @protocol definitions (excluding forward declarations ending with ';')
    # 3. Matches C++ class, struct, or enum definitions, including standard EXPORT macro modifiers
    #    (e.g., 'class BASE_EXPORT MyClass', 'struct COMPONENT_EXPORT(FOO) MyStruct', ignoring forward declarations).
    patterns = [
        re.compile(r'@interface\s+' + re.escape(clean_type) + r'\b'),
        re.compile(r'@protocol\s+' + re.escape(clean_type) + r'\b(?!\s*;)'),
        re.compile(
            r'\b(?:class|struct|enum)\s+(?:[A-Z0-9_]+_EXPORT\b\s+|COMPONENT_EXPORT\([^)]+\)\s+)?'
            + re.escape(clean_type) + r'\b(?!\s*;)')
    ]

    # 3. Fast path: leverage git grep index execution to identify
    # candidate declaring source files instantly
    try:
        grep_res = subprocess.run([
            "git", "grep", "-l", "--extended-regexp",
            f"(@interface|@protocol|class|struct|enum)[[:space:]]+[A-Z0-9_]*[[:space:]]*{re.escape(clean_type)}\\b",
            "--", "*.h"
        ],
                                  cwd=src_root,
                                  capture_output=True,
                                  text=True)
        if grep_res.returncode == 0 and grep_res.stdout.strip():
            candidate_files = grep_res.stdout.strip().splitlines()
            for f_path in candidate_files:
                # Filter out temporary bridging files and restrict validation
                # to targeted shipping paths
                if f_path.endswith("_swift_bridge.h") or not any(
                        f_path.startswith(d)
                        for d in ("ios/", "components/", "base/", "ui/")):
                    continue
                full_path = src_root / f_path
                try:
                    content = full_path.read_text(errors='ignore')
                    # Verify actual declarations to ignore forward-declarations
                    if any(pat.search(content) for pat in patterns):
                        _SYMBOL_CACHE[type_name] = f_path
                        return f_path
                except Exception:
                    pass
    except Exception:
        pass

    # 4. Fallback path: slower exhaustive directory sweeps across
    # platform source layers
    target_dirs = [
        src_root / "ios",
        src_root / "components",
        src_root / "base",
        src_root / "ui"
    ]

    import os  # Keep local import for walk compatibility if needed, but os.walk works with Path objects
    for directory in target_dirs:
        if not directory.exists():
            continue
        for root, _, files in os.walk(directory):
            for file in files:
                if file.endswith(
                        '.h') and not file.endswith('_swift_bridge.h'):
                    full_path = Path(root) / file
                    try:
                        content = full_path.read_text(errors='ignore')
                        if any(pat.search(content) for pat in patterns):
                            rel_path = str(full_path.relative_to(src_root))
                            _SYMBOL_CACHE[type_name] = rel_path
                            return rel_path
                    except Exception:
                        pass

    # Cache negative lookups to guarantee unmapped missing symbols
    # don't degrade secondary execution rounds
    _SYMBOL_CACHE[type_name] = None
    return None


def parse_clang_errors(stderr):
    """Parses Clang compilation error output to extract failing source files and incomplete type names.

    Identifies incomplete type diagnostics and missing nested name identifiers natively
    to isolate candidate types requiring physical header includes.
    """
    errors = []
    current_error_file = None

    # Standardize to list of lines
    for line in stderr.splitlines():
        # Clean ANSI escape codes cleanly
        clean_line = re.sub(r'\x1b\[[0-9;]*m', '', line.strip())

        # Highly robust simplified pattern matching:
        # Matches: file_path:line:col: error: ... incomplete type 'type_name' ...
        # Matches: file_path:line:col: error: ... 'type_name' named in nested name specifier ...
        if ": error: " in clean_line:
            file_path_match = re.match(r'^([a-zA-Z0-9_/\.\-]+):\d+:\d+:',
                                       clean_line)
            if file_path_match:
                current_error_file = file_path_match.group(1)

                # Extract type_name inside single quotes
                type_matches = re.findall(r"'([^']+)'", clean_line)
                if type_matches:
                    # The last or main quoted type name is the incomplete type
                    type_name = type_matches[0]

                    # Skip standard placeholders
                    if type_name in ("incomplete type", "sizeof"):
                        if len(type_matches) > 1:
                            type_name = type_matches[1]

                    if any(pat in clean_line
                           for pat in ("incomplete type",
                                       "nested name specifier",
                                       "undeclared identifier",
                                       "no template named",
                                       "unknown type name", "no type named",
                                       "no member named")):
                        errors.append({
                            "file": current_error_file,
                            "type": type_name
                        })
        elif ": note: " in clean_line and current_error_file:
            # Check if the note mentions an incomplete type passed to a constructor/function
            # E.g., cannot convert argument of incomplete type 'ConnectorsService *'
            # E.g., 'ConnectorsService' is not defined, but forward declared here
            note_match = re.search(r"incomplete type '([^']+)'", clean_line)
            if not note_match:
                note_match = re.search(
                    r"'([^']+)' is not defined, but forward declared",
                    clean_line)
            if note_match:
                type_name = note_match.group(1)
                # Strip any pointer stars, references or spaces
                type_name = type_name.replace(" *", "").replace(
                    "*", "").replace(" &", "").replace("&", "").strip()

                errors.append({"file": current_error_file, "type": type_name})
    return errors


def heal_checkdeps(src_root):
    """Runs Chromium's python checkdeps tool, parses illegal includes, and whitelists them inside corresponding DEPS files.

    Climbs up package directory structures to resolve bounding DEPS boundary artifacts,
    injecting missing '+path/to/header.h' rules alphabetically inside include_rules arrays.
    """
    print("🔍 Running Chromium checkdeps validation...")
    # Run checkdeps tool on modified files compared to origin/main
    modified_files = subprocess.check_output(
        ["git", "diff", "--name-only", "origin/main...HEAD"],
        text=True).split("\n")
    modified_files = [
        f for f in modified_files
        if f and (f.endswith('.mm') or f.endswith('.cc') or f.endswith('.h'))
    ]

    healed = False
    for f_path in modified_files:
        abs_f_path = src_root / f_path
        if not abs_f_path.exists():
            continue

        checkdeps_res = run_cmd(
            ["python3", "buildtools/checkdeps/checkdeps.py", f_path])
        checkdeps_output = checkdeps_res.stdout + "\n" + checkdeps_res.stderr
        if "Illegal include" not in checkdeps_output:
            continue

        # Parse checkdeps output
        # Matches: filepath\n  Illegal include: "path/to/file.h"\n    Because of...
        pattern = r'Illegal include:\s+"([^"]+)"'
        matches = re.findall(pattern, checkdeps_output)

        for illegal_include in matches:
            print(
                f"⚠️ DEPS Include Boundary Violation found in file: {f_path}")
            print(f"  - Illegal include: {illegal_include}")

            # Locate the nearest DEPS file by climbing up the folder tree
            deps_file = None
            for parent in abs_f_path.parents:
                if not parent.is_relative_to(src_root):
                    break
                candidate = parent / "DEPS"
                if candidate.exists():
                    deps_file = candidate
                    break

            if not deps_file:
                print("  ❌ No DEPS file found climbing up the tree.")
                continue

            print(
                f"  🎯 Located DEPS file to allowlist: {deps_file.relative_to(src_root)}"
            )

            # Open and inject allowlist include_rule into DEPS
            content = deps_file.read_text()

            rule = f'  "+{illegal_include}",'
            if rule in content:
                continue

            # Inject alphabetically inside include_rules block
            lines = content.split("\n")
            include_rules_start = -1
            for idx, line in enumerate(lines):
                if "include_rules = [" in line:
                    include_rules_start = idx
                    break

            if include_rules_start != -1:
                inserted = False
                for idx in range(include_rules_start + 1, len(lines)):
                    if "]" in lines[idx]:
                        lines.insert(idx, rule)
                        inserted = True
                        break
                    # Match keep-sorted or alphabetical rules
                    match = re.search(r'"([^"]+)"', lines[idx])
                    if match:
                        path = match.group(1)
                        clean_rule_path = illegal_include.replace("+", "")
                        clean_path = path.replace("+", "")
                        if clean_rule_path < clean_path:
                            lines.insert(idx, rule)
                            inserted = True
                            break
                if inserted:
                    content = "\n".join(lines)
                    deps_file.write_text(content)
                    print(f"  ✨ Successfully healed DEPS allowlists!")
                    healed = True

    return healed


def main():
    """Automated self-healing compiler loop entry point."""
    parser = argparse.ArgumentParser(
        description=
        "Compiler-driven automated self-healing loop for forward declarations."
    )
    parser.add_argument("branch_name",
                        help="Name of the active optimization branch.")
    parser.add_argument(
        "out_dir",
        help=
        "Path to the build output directory (e.g., out/Debug-iphonesimulator)."
    )
    parser.add_argument(
        "--targets",
        nargs="+",
        default=["chrome", "ios_chrome_unittests"],
        help="List of Ninja build targets to compile and validate.")
    args = parser.parse_args()

    branch = args.branch_name
    out_dir = args.out_dir
    targets = args.targets

    # Verify we are on the correct branch
    try:
        active_branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            text=True
        ).strip()
        if active_branch != branch:
            print(
                f"❌ Error: Active git branch ({active_branch}) does not match the specified branch ({branch})."
            )
            sys.exit(1)
    except Exception as e:
        print(f"❌ Error checking active git branch: {e}")
        sys.exit(1)

    # Resolve the repository src root
    try:
        src_root = forward_declarations_lib.find_src_root()
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    max_iterations = 5

    print(
        f"🤖 Starting Compiler & Build-System Self-Healing loop on branch: {branch}..."
    )

    # Pre-flight check: ensure git working directory is clean
    # before starting auto-healing loop
    status_res = run_cmd(["git", "status", "--porcelain"])
    if status_res.stdout.strip():
        print(
            "❌ Error: Git working directory is not clean. Please commit or stash your changes before running self-healing."
        )
        sys.exit(1)

    # Enter feedback loops attempting up to max_iterations
    # of self-correction cycles
    for iteration in range(max_iterations):
        # --- Phase 1: Pre-compilation Build Validation Healing ---
        # 1. Proactively run and heal GN Check target dependencies!
        gn_healed = heal_gn_check.heal_gn_check(src_root, out_dir)

        # 2. Proactively run and heal DEPS checkdeps include boundary violations!
        deps_healed = heal_checkdeps(src_root)

        # Amend the active layout commit instantly if configuration
        # allowlists were modified
        if gn_healed or deps_healed:
            print(
                "💾 Committing build-system allowlists healing onto the active branch..."
            )
            run_cmd(["git", "add", "-u"])
            run_cmd(["git", "commit", "--amend", "--no-edit"])
            print("✅ Build-system heal commit successfully amended!")

        # --- Phase 2: Native Compilation Lifecycle ---
        # 3. Compile shipping and test targets using continuous Ninja builds
        print(
            f"\n🔨 [Iteration {iteration+1}/{max_iterations}] Compiling targets: {' '.join(targets)}..."
        )
        build_res = run_cmd(["autoninja", "-C", out_dir] + targets)

        if build_res.returncode == 0:
            # Validate final gn check hygiene dynamically to guarantee success
            if run_cmd(["gn", "check", out_dir]).returncode == 0:
                print(
                    f"✅ branch {branch} compiled & validated with 100% SPOTLESS SUCCESS!"
                )
                sys.exit(0)

        print("⚠️ Compilation failed. Analyzing Clang error logs...")
        # Consolidate compiler terminal buffer streams to evaluate errors
        combined_output = build_res.stdout + "\n" + build_res.stderr
        errors = parse_clang_errors(combined_output)

        if not errors:
            print(
                "❌ No auto-healable Clang errors found in compiler output. Manual intervention required."
            )
            print(f"Raw Compiler Error Snippet:\n{combined_output[:1000]}")
            sys.exit(1)

        print(
            f"🔍 Found {len(errors)} auto-healable compile failures. Resolving..."
        )
        healed_files = []

        # --- Phase 3: Header Remediation Injections ---
        for err in errors:
            # Map relative diagnostic paths out to absolute repository sources
            src_file = (Path(out_dir) / err["file"]).resolve()
            type_name = err["type"]

            print(f"  - Failing file: {src_file.relative_to(src_root)}")
            print(f"  - Incomplete C++ type: {type_name}")

            # Locate actual declaring interface file using cache/grep tools
            required_header = find_header_for_type(src_root, type_name)
            if not required_header:
                print(
                    f"  ❌ Could not locate declaration header for type '{type_name}' in the repository."
                )
                continue

            print(
                f"  🎯 Located required declaration header: {required_header}")

            content = src_file.read_text(errors='ignore')

            # Determine runtime syntax directives enforcing Objective-C imports
            is_objc = src_file.suffix in ('.mm', '.m') or '/ios/' in str(src_file)
            healed_content = forward_declarations_lib.insert_sorted_import(
                content, required_header, is_objc=is_objc)

            if is_objc:
                healed_content = forward_declarations_lib.heal_includes_to_imports(
                    healed_content)

            src_file.write_text(healed_content)

            # Enforce physical code style standard formatting directly
            # on the repaired source node
            subprocess.run(["git", "cl", "format", str(src_file)],
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
            print(
                f"  ✨ Successfully auto-healed and formatted {src_file.name}!"
            )
            healed_files.append(src_file)

        if not healed_files:
            print(
                "❌ None of the compile failures could be auto-healed. Aborting loop."
            )
            sys.exit(1)

        # Cleanly fold physical source repairs directly into the active commit
        print("💾 Committing compiler healing fixes onto the active branch...")
        run_cmd(["git", "add", "-u"])
        run_cmd(["git", "commit", "--amend", "--no-edit"])
        print("✅ Compiler heal commit successfully amended!")

    print(
        f"❌ Reached maximum iterations ({max_iterations}) without achieving 100% compile success. Manual intervention required."
    )
    sys.exit(1)


if __name__ == "__main__":
    main()
