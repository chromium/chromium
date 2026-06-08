# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CLI tool to automatically fix missing GN dependencies from gn check failures."""

import argparse
from pathlib import Path
import re
import subprocess
import sys

import forward_declarations_lib


def run_cmd(args):
    """Runs a shell command synchronously and returns the result."""
    return subprocess.run(args, capture_output=True, text=True)


def heal_gn_check(src_root, out_dir):
    """Runs gn check, parses target dependency errors, and automatically appends missing deps to BUILD.gn files.

    Resolves source target boundary contexts and inserts missed dependencies alphabetically
    into corresponding 'deps = [...]' arrays to dynamically auto-correct BUILD violations.
    """
    print("🔍 Running GN dependency allowlist check...")
    gn_res = run_cmd(["gn", "check", out_dir])
    if gn_res.returncode == 0:
        print("✅ gn check passed perfectly!")
        return False

    matches = re.finditer(
        r'ERROR at (//[^:]+):\d+:\d+:\s+Can\'t include this header[^\n]+The target:\s+([^ ]+)\s+is including a file from the target:\s+([^ ]+)',
        gn_res.stderr, re.MULTILINE)

    healed = False
    for m in matches:
        header_file = m.group(1)
        src_target = m.group(
            2
        )  # e.g. //ios/chrome/browser/download/ui/download_list:download_list
        dest_target = m.group(3)  # e.g. //ios/web/public/download:download

        print(f"⚠️ GN Dependency Violation found in header: {header_file}")
        print(f"  - Source Target: {src_target}")
        print(f"  - Missing Dependency: {dest_target}")

        # Parse the source target components
        target_path, target_name = src_target.split(":")
        # Convert target path to BUILD.gn path
        build_gn_path = src_root / target_path.replace("//", "") / "BUILD.gn"

        if not build_gn_path.exists():
            print(f"  ❌ BUILD.gn not found at path: {build_gn_path}")
            continue

        # Open and heal BUILD.gn
        content = build_gn_path.read_text(errors='ignore')

        # We search for the source_set or target containing target_name, and inject the dependency
        # A precise target finder matching target_name
        target_pattern = r'(\b\w+\(\s*"' + re.escape(
            target_name) + r'"\s*\)\s*\{[^}]*?deps\s*=\s*\[)'

        if re.search(target_pattern, content):
            # Inject the allowlisted target into deps block
            # We strip :dest_name suffix if it is redundant with directory name (standard Chromium style)
            dep_dir, dep_name = dest_target.split(":")
            clean_dep = dep_dir if dep_dir.endswith(
                "/" + dep_name) else dest_target

            print(
                f"  🎯 Injecting dependency target \"{clean_dep}\" into: {build_gn_path.relative_to(src_root)}"
            )

            # We insert the dependency alphabetically into the deps block
            lines = content.split("\n")
            target_idx = -1
            deps_start_idx = -1

            # Scan lines to locate the correct target's deps block
            for idx, line in enumerate(lines):
                if re.search(
                        r'\b\w+\(\s*"' + re.escape(target_name) + r'"\s*\)',
                        line):
                    target_idx = idx
                if target_idx != -1 and "deps = [" in line:
                    deps_start_idx = idx
                    break

            if deps_start_idx != -1:
                # Check if it is a single-line empty deps, e.g., "deps = []"
                if "deps = []" in lines[deps_start_idx]:
                    lines[deps_start_idx] = lines[deps_start_idx].replace(
                        "deps = []", f'deps = [\n    "{clean_dep}",\n  ]')
                    content = "\n".join(lines)
                    build_gn_path.write_text(content)
                    print(f"  ✨ Successfully healed BUILD.gn dependencies!")
                    healed = True
                else:
                    # Alphabetically inject
                    inserted = False
                    for idx in range(deps_start_idx + 1, len(lines)):
                        if "]" in lines[idx]:
                            lines.insert(idx, f'    "{clean_dep}",')
                            inserted = True
                            break
                        match = re.search(r'"([^"]+)"', lines[idx])
                        if match:
                            path = match.group(1)
                            if clean_dep < path:
                                lines.insert(idx, f'    "{clean_dep}",')
                                inserted = True
                                break
                    if inserted:
                        content = "\n".join(lines)
                        build_gn_path.write_text(content)
                        print(f"  ✨ Successfully healed BUILD.gn dependencies!")
                        healed = True

    return healed


def main():
    if sys.platform == 'win32':
        print("Error: This script is not supported on Windows.")
        return 1

    parser = argparse.ArgumentParser(
        description="Runs gn check and automatically adds missing dependencies to BUILD.gn files."
    )
    parser.add_argument("out_dir",
                        help="Path to the build output directory (e.g., out/Debug-iphonesimulator).")
    args = parser.parse_args()

    try:
        src_root = forward_declarations_lib.find_src_root()
    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1

    heal_gn_check(src_root, args.out_dir)
    return 0


if __name__ == '__main__':
    sys.exit(main())
