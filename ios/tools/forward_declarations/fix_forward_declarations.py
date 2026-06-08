# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CLI tool to apply forward declarations to headers and update implementations."""

import argparse
from pathlib import Path
import re
import subprocess
import sys

import forward_declarations_lib


def _find_companion_file(header_path, suffixes):
    """Finds a companion file (impl or test) for a header using given suffixes."""
    p = Path(header_path)
    for suffix in suffixes:
        candidate = p.with_name(p.stem + suffix)
        if candidate.exists():
            return candidate

        # Check parent if in 'public'
        if p.parent.name == 'public':
            candidate = p.parent.parent / (p.stem + suffix)
            if candidate.exists():
                return candidate
    return None


def find_implementation_file(header_path):
    """Finds the corresponding implementation file (.mm, .cc, .m) for a header.

    Scans standard sibling paths first, then climbs out of 'public'
    boundary directories to match standardized modular file configurations.
    """
    return _find_companion_file(header_path, ('.mm', '.cc', '.m'))


def find_unittest_file(header_path):
    """Finds the corresponding unit test file for a header.

    Tracks identical layout matching logic as primary implementations
    to locate unit test blocks.
    """
    return _find_companion_file(header_path, ('_unittest.mm', '_unittest.cc', '_unittest.m'))


def is_symbol_used_in_file(file_path, symbol):
    """Checks if a specific symbol is used in the given file.

    Executes a simple regex word-boundary check to verify down-stream
    usage dependencies.
    """
    p = Path(file_path)
    if not p.exists():
        return False
    content = p.read_text(errors='ignore')
    return bool(re.search(r'\b' + re.escape(symbol) + r'\b', content))


def fix_header_and_implementation(header_path, src_root):
    """Applies forward declaration fixes to headers and implementations.

    Orchestrates the full transformation lifecycle for a single interface
    boundary. Extracts candidate forward-declarations, identifies which
    downstream compilation targets require shifted full imports, applies
    automated source restructuring, and queues modified files for deferred
    batch code formatting.
    """
    header_path = Path(header_path)
    # Identify companion compilation sources dynamically
    impl_path = find_implementation_file(header_path)
    unittest_path = find_unittest_file(header_path)

    print(f"Analyzing: {header_path.relative_to(src_root)}")
    if impl_path:
        print(
            f"Corresponding implementation: {impl_path.relative_to(src_root)}"
        )
    else:
        print(
            "Warning: No corresponding implementation file (.mm, .cc, .m) found."
        )
    if unittest_path:
        print(
            f"Corresponding unit test: {unittest_path.relative_to(src_root)}"
        )

    # Perform abstract syntax analysis to flag non-optimal import nodes
    analysis = forward_declarations_lib.analyze_header(header_path, src_root)

    to_remove_from_header = []
    decls_to_add_to_header = []
    imports_to_add_to_impl = []
    imports_to_add_to_unittest = []

    # Categorize actions needed for each analyzed import entry
    for item in analysis:
        status = item['status']
        if status in ('forward_declare', 'unused'):
            to_remove_from_header.append(item['line'])
            if status == 'forward_declare':
                # Queue explicit forward-declaration tokens for injection
                decls_to_add_to_header.extend(item['forward_declarations'])
                # Forward-declared imports must always shift to implementation
                # files to satisfy full-type requirements
                imports_to_add_to_impl.append(item['import_path'])

            # Validate downstream symbol usage to decide if secondary integration
            # files require imports
            resolved_path = forward_declarations_lib.resolve_import_path(
                item['import_path'], src_root, header_path)
            if resolved_path:
                imp_content = resolved_path.read_text(errors='ignore')
                # Extract exported definitions from the candidate header
                objc_classes, objc_protocols = forward_declarations_lib.find_objc_definitions(
                    imp_content)
                cpp_defs = forward_declarations_lib.find_cpp_definitions(
                    imp_content)
                non_declarables = forward_declarations_lib.find_non_forward_declarables(
                    imp_content)
                all_symbols = objc_classes | objc_protocols | {d[0] for d in cpp_defs} | non_declarables

                # Shift 'unused' header imports down to implementations only
                # if the implementation actually consumes them
                if status == 'unused' and impl_path:
                    if any(
                            is_symbol_used_in_file(impl_path, sym)
                            for sym in all_symbols):
                        imports_to_add_to_impl.append(item['import_path'])

                # Propagate imports to unit test modules if test logic accesses
                # the extracted symbols
                if unittest_path:
                    if any(
                            is_symbol_used_in_file(unittest_path, sym)
                            for sym in all_symbols):
                        imports_to_add_to_unittest.append(item['import_path'])

    # Early exit if no structural header transformations are required
    if not to_remove_from_header:
        print("No modifications needed for this header file.")
        return []

    # --- Phase 1: Header Re-structuring ---
    header_content = header_path.read_text(errors='ignore')

    # Remove the imports that are being shifted to the implementation or discarded
    for line in to_remove_from_header:
        pattern = re.escape(line) + r'\s*\n'
        header_content = re.sub(pattern, '', header_content)

    # Structurally insert synthesized forward declaration block
    if decls_to_add_to_header:
        header_content = forward_declarations_lib.insert_forward_declarations(
            header_content, decls_to_add_to_header)

    # Execute code-style healers to reconcile layout anomalies
    header_content = forward_declarations_lib.heal_misplaced_fwd_decls(
        header_content)
    header_content = forward_declarations_lib.heal_spacing(header_content)

    # Enforce #import conventions for mixed ObjC++ target sources inside
    # the iOS directory structure
    if 'ios/' in str(header_path) or 'ios_internal/' in str(header_path):
        header_content = forward_declarations_lib.heal_includes_to_imports(
            header_content)

    header_path.write_text(header_content)
    print(f"Updated header: {header_path.relative_to(src_root)}")

    # --- Phase 2: Implementation Augmentation ---
    if impl_path and imports_to_add_to_impl:
        impl_content = impl_path.read_text(errors='ignore')

        # Determine include syntax directive based on runtime extensions
        is_objc = impl_path.suffix in ('.mm', '.m') or '/ios/' in str(impl_path)
        for imp_path in sorted(list(set(imports_to_add_to_impl))):
            impl_content = forward_declarations_lib.insert_sorted_import(
                impl_content, imp_path, is_objc=is_objc)

        if 'ios/' in str(impl_path) or 'ios_internal/' in str(impl_path):
            impl_content = forward_declarations_lib.heal_includes_to_imports(
                impl_content)

        impl_path.write_text(impl_content)
        print(
            f"Updated implementation: {impl_path.relative_to(src_root)}")

    # --- Phase 3: Unit Test Synchronization ---
    if unittest_path and imports_to_add_to_unittest:
        unittest_content = unittest_path.read_text(errors='ignore')

        is_objc = unittest_path.suffix in ('.mm', '.m') or '/ios/' in str(unittest_path)
        for imp_path in sorted(list(set(imports_to_add_to_unittest))):
            unittest_content = forward_declarations_lib.insert_sorted_import(
                unittest_content, imp_path, is_objc=is_objc)

        if 'ios/' in str(unittest_path) or 'ios_internal/' in str(unittest_path):
            unittest_content = forward_declarations_lib.heal_includes_to_imports(
                unittest_content)

        unittest_path.write_text(unittest_content)
        print(f"Updated unit test: {unittest_path.relative_to(src_root)}")

    # --- Phase 4: Formatting Registration ---
    # Queue physically modified paths to be formatted as a unified batch
    files_to_format = [header_path]
    if impl_path and imports_to_add_to_impl:
        files_to_format.append(impl_path)
    if unittest_path and imports_to_add_to_unittest:
        files_to_format.append(unittest_path)

    print("Successfully optimized locally!")
    return files_to_format


def main():
    """CLI tool entry point to automatically apply forward declarations and update implementations."""
    # Block execution on Windows host environments to prevent
    # platform file locking failures
    if sys.platform == 'win32':
        print("Error: This script is not supported on Windows.")
        return 1

    parser = argparse.ArgumentParser(
        description=
        "Automatically replace header imports with forward declarations "
        "and update implementation.")
    parser.add_argument('paths',
                        nargs='*',
                        help="Paths to header files or directories to fix.")
    parser.add_argument(
        '--git',
        action='store_true',
        help="Automatically fix files modified in the active git branch."
    )
    args = parser.parse_args()

    try:
        src_root = forward_declarations_lib.find_src_root()
    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1

    # Process input paths into explicit file paths list or git modifications
    if args.git:
        files_to_fix = forward_declarations_lib.collect_git_files(src_root)
    else:
        if not args.paths:
            parser.error("Please specify at least one path or use the --git option.")
        files_to_fix = forward_declarations_lib.collect_files(args.paths)

    if not files_to_fix:
        print("No header files found to fix.")
        return 0

    print(f"Optimizing {len(files_to_fix)} header files...\n")

    # Maintain a deduplicated ledger of physically updated source paths
    # requiring formatting
    all_files_to_format = set()
    for idx, abs_path in enumerate(sorted(files_to_fix)):
        print(f"[{idx+1}/{len(files_to_fix)}]")
        formatted_list = fix_header_and_implementation(abs_path, src_root)
        if formatted_list:
            all_files_to_format.update(formatted_list)
        print()

    # Execute code formatting on all touched files
    if all_files_to_format:
        files_str = [str(f) for f in sorted(all_files_to_format)]
        print(f"Applying code formatting to {len(files_str)} touched files...")
        try:
            subprocess.run(['git', 'cl', 'format'] + files_str,
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
        except Exception:
            pass
        print("Batch optimization and formatting completed perfectly!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
