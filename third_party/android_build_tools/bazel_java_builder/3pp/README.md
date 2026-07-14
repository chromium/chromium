# Bazel Java Builder Patches

This directory contains patches applied to `BazelJavaBuilder` (built from the
Bazel source tree) before it is packaged by the 3pp recipe.

The current patch implements class-level dependency tracking for Chromium's
Java compilation.

## How to Modify the Code and Re-create the Patch

### 1. Checkout Bazel Source

```bash
# Clone the Bazel repository
git clone https://github.com/bazelbuild/bazel.git
cd bazel

# Checkout the version tag set in 3pp.pb
git checkout 3.7.2
```

### 2. Apply the Existing Patch

```bash
# From the root of the bazel repository:
git apply ../patches/0001-Add-class-level-dependency-tracking.patch
```

### 3. Make Your Changes

Edit the files in the Bazel repository. The relevant files are:
*   `src/java_tools/buildjar/java/com/google/devtools/build/buildjar/BazelJavaBuilder.java`
*   `src/java_tools/buildjar/java/com/google/devtools/build/buildjar/javac/plugins/dependency/DependencyModule.java`
*   `src/java_tools/buildjar/java/com/google/devtools/build/buildjar/javac/plugins/dependency/ClassDependenciesPlugin.java`
    (New file)

### 4. Regenerate the Patch

After making changes, regenerate the patch file back into the Chromium tree:

```bash
# From the root of the bazel repository, generate a diff against the tag:
git diff 3.7.2 > ../patches/0001-Add-class-level-dependency-tracking.patch
```

Ensure the patch file is staged and committed in your Chromium branch.

### 5. Update the 3pp Build version

If you want the changes to be built by the bots, you must increment
`patch_version` in `3pp/3pp.pb`.
