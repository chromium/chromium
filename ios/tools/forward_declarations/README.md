# Chrome for iOS: Forward Declarations & Self-Healing Optimization Guide

This guide documents the **Automated Forward Declarations Optimization & Compiler Self-Healing** workflow for Chrome for iOS. It is designed to guide future engineering agents and human developers in optimizing C++ and Objective-C++ headers, decoupling interface dependencies, and resolving compilation issues automatically.

---

## 📖 Overview

C++ `#include` and Objective-C `#import` statements in header files (`.h`) create tight compile-time coupling. When a header file is modified, all files transitively including it must be recompiled.

By replacing full header imports with lightweight C++ forward declarations (`class` / `struct`) and Objective-C forward declarations (`@class` / `@protocol`), we:
1. **Decouple Interface Dependencies** — Reducing downstream build breaks.
2. **Speed up Compilation** — Siso/Ninja spend less time parsing redundant AST nodes.
3. **Maintain Clean Scope** — Move implementation imports directly to `.mm` and `.cc` files where they belong.

---

## 🛠️ The Tool Suite

Located in `ios/tools/forward_declarations/`, the optimization toolchain consists of:

1. **`check_forward_declarations.py`**:
   * Recursively scans directories or Git modified files (`--git` option) to identify header files with optimization opportunities.
   * Flags redundant imports and recommends exact forward declaration signatures.
2. **`fix_forward_declarations.py`**:
   * Automatically patches headers by removing redundant `#import` lines and inserting appropriate forward declaration blocks. Supports individual files, full recursive directory walking, or Git modified files (`--git` option).
   * Shifts the original imports into corresponding implementation (`.mm`/`.cc`) and test (`_unittest.mm`) files to satisfy compiler usage.
3. **`forward_declarations_lib.py`**:
   * The core lexical parsing library that handles code style formatting. Loads highly common base/system header definitions from a customizable `core_header_mapping.json` configuration file.
   * Includes **Style Healers** to merge adjacent duplicate namespaces, clean up spacing, and convert includes to imports.
4. **`self_heal_branch.py`**:
   * A compiler-driven automated healing loop.
   * Runs customizable compilation targets (defaults to `chrome`, `ios_chrome_unittests`) recursively, parses Clang compiler diagnostics, resolves incomplete type errors via intelligent symbol caching, and automatically allowlists GN/DEPS include boundary violations.

---

## 🚀 The Optimization Workflow (Step-by-Step)

Follow this systematic workflow to optimize any folder under `//ios/chrome/`:

### Step 1: Branch Setup
Always isolate your optimization work inside a clean Git branch.
```sh
git checkout -b fwd-decl-<feature-name>-pilot
```

### Step 2: Scan and Analyze Opportunities
Run the scanner on your target subdirectory (e.g., `ios/chrome/browser/enterprise`) or on files modified in your git branch to see recommended optimizations:
```sh
# Scan a specific directory
python3 ios/tools/forward_declarations/check_forward_declarations.py ios/chrome/browser/enterprise

# Or scan files modified in your branch compared to origin/main automatically
python3 ios/tools/forward_declarations/check_forward_declarations.py --git
```

### Step 3: Apply Automatic Header Replacements
Apply the scan recommendations to auto-replace header imports with forward declarations and move imports into implementation files:
```sh
# Fix a specific directory
python3 ios/tools/forward_declarations/fix_forward_declarations.py ios/chrome/browser/enterprise

# Or fix files modified in your branch compared to origin/main automatically
python3 ios/tools/forward_declarations/fix_forward_declarations.py --git
```
*This will stage initial changes across your header and source files.*

### Step 4: Commit the Initial Layout
Create an initial commit containing the basic layout transformations:
```sh
git add -A
git commit -m "[ios] Optimize forward-declarations in <feature> (pilot)"
```

### Step 5: Run the Automated Self-Healing Loop
Because header decoupling can lead to missing transitive includes inside downstream files, launch the **Self-Healing compiler loop** to automatically diagnose and patch compilation failures:
```sh
python3 ios/tools/forward_declarations/self_heal_branch.py fwd-decl-<feature-name>-pilot out/Debug-iphonesimulator --targets chrome ios_chrome_unittests
```
* **What it does**:
  1. Runs `buildtools/checkdeps/checkdeps.py` to identify illegal includes, automatically allowlisting them inside local `DEPS` files.
  2. Runs `gn check` to detect missing target dependencies.
  3. Compiles specified Ninja build targets.
  4. If Clang throws incomplete type failures, it resolves the correct header defining the type via `core_header_mapping.json` or efficient repository scanning, inserts the import into the implementation file, and auto-commits the healing step onto the branch!
  5. Repeats until a **100% spotless compilation** is achieved.

### Step 6: Run Formatting & Verification
Format the code and run unit tests to ensure everything is 100% pristine:
```sh
# Apply clang-format to all touched lines
git cl format
git add -u
git commit --amend --no-edit

# Run affected unit tests
python3 ios/tools/run_cl_tests.py
```

### Step 7: Upload the CL
Finally, upload the optimized CL to Gerrit for review:
```sh
GIT_EDITOR=true git cl upload
```

---

## 💡 Critical Lessons & Edge Cases (For Future Agents)

When modifying the script logic or performing manual interventions, pay attention to these core rules:

1. **Template Incomplete Types**:
   C++ template constructors (such as `std::unique_ptr<Type>` or `absl::variant<Type>`) require the full C++ type definition rather than just a forward declaration. If Clang throws an incomplete type failure inside a constructor/destructor, import the header inside the `.mm` file rather than trying to forward declare it in the `.h`.
2. **`DEPS` Allowlist Boundaries**:
   Under Chromium's architecture, directories restrict imports to enforce clean layers. If you move an import to an implementation file and `checkdeps` raises an `Illegal include` error, add the correct import pattern (`+path/to/header.h`) to the directory's local `DEPS` file.
3. **Alphabetical Spacing & Sorting**:
   Keep all `#import` and `DEPS` lines sorted alphabetically. Our style healer `heal_spacing` dynamically maintains this formatting.
4. **Adjacent Namespace Merging**:
   When forward declarations are generated, they are placed inside a namespace block matching their type (e.g., `namespace enterprise_idle { class IdleService; }`). Ensure adjacent duplicate namespaces are merged into a single block for maximum readability.
