# Managing Chromium dependencies

Chromium uses `gclient` (part of depot_tools) to manage dependencies (e.g. V8,
WebRTC). Information such as URLs and hashes is stored in the `DEPS` file
located in the root of the project. In addition to `DEPS`, `gclient` may read
git submodules (see
[depot_tools submodules support](https://docs.google.com/document/d/1N_fseFNOj10ETZG3pZ-I30R__w96rYNtvx5y_jFGJWw/view)).

`gclient` supports three dependency types: git, [gcs](gcs_dependencies.md), and
[cipd](cipd_and_3pp.md).

## Adding to GoB

If the code is in a Git repo that you want to mirror, please file an [infra git
ticket](https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git)
to get the repo mirrored onto chromium.googlesource.com; we don't allow direct
dependencies on non-Google-hosted repositories, so that we can still build
if an external repository goes down.

Once the mirror is set up, you can add the actual dependency into `DEPS`.

## Adding dependencies

Add your entry in `DEPS`. Then run `gclient gitmodules` to generate git
submodules; this will contain the `.gitmodule` change and gitlink. Edit the
`OWNERS` file and add the gitlink path. Then, run `git add DEPS OWNERS` to stage
those files for commit, followed by `git commit`. Your change is now ready to be
sent for a review using `git cl upload`.

For example, if your new dependency is "src/foo/bar.git", its gitlink path is
"foo/bar", and the top level `OWNERS` entry is `per-file foo/bar=*`. You can
confirm this by running `git status`. [Example CL](https://crrev.com/c/4923074).

```
# manual edit of DEPS and OWNERS file (see changes below).

 % gclient gitmodules
.gitmodules and gitlinks updated. Please check `git diff --staged`and commit those staged changes (`git commit` without -a)

 % git add OWNERS DEPS # stage files

 % git diff --cached # see staged changes
diff --git a/.gitmodules b/.gitmodules
index 29c355fa92e3d..89866442d45aa 100644
--- a/.gitmodules
+++ b/.gitmodules
@@ -1,3 +1,6 @@
+[submodule "foo/bar"]
+       path = foo/bar
+       url = https://chromium.googlesource.com/foo/bar.git
 [submodule "third_party/clang-format/script"]
        path = third_party/clang-format/script
        url = https://chromium.googlesource.com/external/github.com/llvm/llvm-project/clang/tools/clang-format.git
diff --git a/DEPS b/DEPS
index 44fbc53a0d53a..05481be5066ed 100644
--- a/DEPS
+++ b/DEPS
@@ -555,6 +555,10 @@ allowed_hosts = [
 ]

 deps = {
+  'src/foo/bar': {
+      'url': Var('chromium_git') + '/foo/bar.git' + '@' +
+        '1111111111111111111111111111111111111111',
+  },
   'src/third_party/clang-format/script':
     Var('chromium_git') +
     '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@' +
diff --git a/OWNERS b/OWNERS
index 55bfe60fcb03b..02b4117fca1ea 100644
--- a/OWNERS
+++ b/OWNERS
@@ -37,6 +37,7 @@ per-file README.md=*
 per-file WATCHLISTS=*

 # git submodules
+per-file foo/bar=*
 per-file third_party/clang-format/script=*
 per-file chrome/browser/resources/preinstalled_web_apps/internal=*
 per-file chrome/installer/mac/third_party/xz/xz=*
diff --git a/foo/bar b/foo/bar
new file mode 160000
index 0000000000000..1111111111111
--- /dev/null
+++ b/foo/bar
@@ -0,0 +1 @@
+Subproject commit 1111111111111111111111111111111111111111

 % git status
On branch test_newdep
Your branch is up to date with 'origin/main'.

Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
        modified:   .gitmodules
        modified:   DEPS
        modified:   OWNERS
        new file:   foo/bar

Changes not staged for commit:
  (use "git add/rm <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
        deleted:    foo/bar


% # At this point, you can run gclient sync if you want to get the dependency.
% # But it's not required, and you can use `git cl upload`.

 % git commit -m "[DEPS] Example of new dependency"
[test_newdep 9731cfb680756] [DEPS] Example of new dependency
 4 files changed, 9 insertions(+)
 create mode 160000 foo/bar

 % git cl upload
Found change with 1 commit...
Running Python 3 presubmit upload checks ...
-- snip --
remote:   https://chromium-review.googlesource.com/c/chromium/src/+/4923074 [DEPS] Example of new dependency [NEW]
-- snip --
```

## Making changes to dependencies {#changing-dependencies}

If you need a change in a dependency, the general process is to first contribute
the change upstream, then [roll into Chromium](#rolling-dependencies). Some
projects (e.g. Skia) are autorolled, but it is good practice to manually roll
after an upstream change to ensure your change can be successfully rolled and
there are no resulting compile or test failures.

Upstream projects have a variety of contribution workflows. The two most common
are Gerrit-based reviews using `git cl upload` (like Chromium itself) and GitHub
PRs. Some projects have a `CONTRIBUTING.md` file in their root that gives
instructions.

In most cases, creating a standalone checkout/clone of the project you're
modifying, outside your Chromium checkout, is the best way to ensure you're
contributing to upstream `HEAD` and can run the project's presubmit checks.
Follow the project's contribution instructions (e.g. running `fetch` or
`gclient sync` as needed, possibly after downloading or cloning the source). If
you do attempt to create and upload changes directly inside submodules in your
Chromium checkout, be careful not to commit the new submodule hashes to any
Chromium changes. You may also need to
[create symlinks to enable other projects' presubmits](#presubmit-symlinks), or
else skip them by uploading with `--bypass-hooks`.

## Rolling dependencies

### Using gclient

If you want to roll dependency to a specific version, you can do by running the
following:

```
gclient setdep -r {path to dependency}@{hash}
```

For example, let's say you want to roll boringssl in chromium/src to commit
e4acd6cb568214b1c7db4e59ce54ea2e1deae1f5. You would run the following:

```
gclient setdep -r src/third_party/boringssl/src@e4acd6cb568214b1c7db4e59ce54ea2e1deae1f5
```

Under the hood, gclient understands DEPS file, and knows what needs to update.
In the example above, it actually updates boringssl_revision variable that is
used in boringssl deps declaration.

Example of DEPS file: `vars = { 'boringssl_git':
'https://boringssl.googlesource.com', 'boringssl_revision':
'e4acd6cb568214b1c7db4e59ce54ea2e1deae1f5', } deps = {
'src/third_party/boringssl/src': Var('boringssl_git') + '/boringssl.git' + '@' +
Var('boringssl_revision'), }`

It also updates gitlink if git submodules are used. Git status will show the
following:

```
$ git status
-- snip --
Changes to be committed:
        modified:   DEPS
        modified:   third_party/boringssl/src
```

### Using roll-dep

depot_tools also provides a `roll-dep` script which can roll the desired
repository to the latest commit on main branch. `roll-dep` handles both DEPS and
git submodules.

### Manual roll / low level roll

You can update things yourself by modifying the DEPS file directly. If git
submodules are used, you also need to update gitlinks - an entry for submodules
in the git database by using:

```
git update-index --add --cacheinfo 160000,{hash},{path}
```

git update-index instructs git to register git submodule change to the index (ie
stages submodule for commit). Particularly, --cacheinfo argument instructs git
to directly insert the specified info into the index: 160000 is gitlink mode
(used by git submodules), {hash} is a new commit hash you want to roll, and path
is relative path to git submodule.

Using the boringssl example above, the following will need to be run inside
chromium/src worktree:

```
git update-index --add --cacheinfo 160000,e4acd6cb568214b1c7db4e59ce54ea2e1deae1f5,third_party/boringssl/src
```

Once executed, `git status` will report there is an update to the submodule,
which you can commit as any other file change.

Alternatively, you can regenerate git submodules once you update DEPS file (see
section below).

## Deleting dependencies

gclient doesn't provide a way to delete dependencies. You can delete dependency
by manually editing DEPS file and running the following to update git
submodules:

```
export DEPENDENCY={dependency}
git rm --cached "$DEPENDENCY"
git config -f .gitmodules --remove-section "submodule.$DEPENDENCY"
git add .gitmodules
```

Using the example from the previous section:

```
export DEPENDENCY=third_party/boringssl/src
git rm --cached "$DEPENDENCY"
git config -f .gitmodules --remove-section "submodule.$DEPENDENCY"
git add .gitmodules
```

Once the commands are executed, you can proceed with committing your change. You
should see change in DEPS, .gitmodule and {dependency}.

## Regenerating git submodules

If there are many git dependency changes to DEPS file, it may be impractical to
manually update-index. For those reasons, gclient provides a convenient way to
regenerate git modules entries and to update .gitmodules file.

Once you are done with your DEPS modifications, run the following script in the
root of you project:

```
gclient gitmodules
```

The script will create a new .gitmodules files and update all gitlinks. Please
note that old gitlinks won't be deleted, and you will need to remove them
manually (see section above for deleting dependencies).

## Modularity and the `checkdeps` Tool

While the root `DEPS` file manages external repositories, Chromium's internal
modularity is enforced by a tool called `checkdeps`. This tool ensures that code
in one component does not improperly include headers from another, helping to
maintain a clean and layered architecture.

If you see an error like this during a build, it's from `checkdeps`:

```
ERROR at //some/component/foo.cc:10:11: Include not allowed.
#include "another/component/bar.h"
^---------------------------------
The include file is not allowed to be included from the current file.
```

This error is controlled by special `DEPS` files located within subdirectories
of the source tree (e.g., `//components/component_a/DEPS`).

### Understanding In-Tree `DEPS` Files

These `DEPS` files are much simpler than the root `DEPS` file. Their primary
purpose is to define header inclusion rules.

*   **`include_rules`:** This is the most important variable. It's a list of
    strings that define which directories are allowed to be included by files
    within the current directory (and its subdirectories).
    *   `"+//path/to/allowed/dir"`: **Allows** including headers from this
        directory.
    *   `"-//path/to/forbidden/dir"`: **Forbids** including headers from this
        directory. This is useful for creating exceptions to a broader rule.
    *   `"!"`: A special rule that stops the `checkdeps` tool from looking in
        parent directories for more `DEPS` files to apply.

*   **`specific_include_rules`:** This works like `include_rules` but applies
    only to files directly within the directory containing the `DEPS` file, not
    to its subdirectories.

### How Rules are Evaluated

When `checkdeps` runs on a file, it:
1.  Looks for a `DEPS` file in the same directory.
2.  If found, it checks the `include_rules`.
3.  If no `DEPS` file is found, or if one is found but doesn't contain the `!`
    rule, it walks up to the parent directory and repeats the process,
    accumulating rules.
4.  This continues until it reaches the source root.

### Diagnosing and Fixing `checkdeps` Violations

1.  **Analyze the Error Message:** The `checkdeps` error is highly informative.
    It tells you the exact file, the problematic include, and often the
    specific rule that caused the violation. Look for a "Because of..." clause:
    ```
    Error: //some/component/foo.cc:10:11: Include not allowed.
    #include "another/component/bar.h"
    ^---------------------------------
    The include file is not allowed to be included from the current file.
    It is not in any dependency of
    //some/component:my_target
    Because of "-another/component" in //some/component/DEPS:6
    ```
    This tells you everything you need to know:
    *   `//some/component/foo.cc` is the file with the bad include.
    *   `"another/component/bar.h"` is the problematic include.
    *   The build target is `//some/component:my_target`.
    *   The specific rule causing the failure is `"-another/component"` on line 6
        of `//some/component/DEPS`.

2.  **Identify the Source and Target:** The error message tells you which file
    (`//some/component/foo.cc`) is trying to include which header
    (`"another/component/bar.h"`).
3.  **Find the Governing `DEPS` File:** If the error message doesn't specify
    the file, start in the directory of the source file (`//some/component/`)
    and look for a `DEPS` file. If you don't find one, look in its parent
    directory, and so on.
4.  **Analyze the Rules:** Read the `include_rules` in the `DEPS` file you
    found. The header you are trying to include is likely not covered by an `+`
    rule, or is explicitly forbidden by a `-` rule.
5.  **Fix the Violation:**
    *   **Best Fix:** Can you achieve your goal *without* adding this new
        dependency? Reusing existing abstractions is always preferred.
    *   **Good Fix:** If the dependency is necessary, you often need to add the
        dependency to the `deps` list of the target that is trying to include
        the header. In the example above, you would edit
        `//some/component/BUILD.gn` and add `//another/component` to the `deps`
        of the `:my_target` target.
    *   **Architectural Fix:** In some cases, you may also need to add a new
        `"+//another/component"` rule to the appropriate `DEPS` file if the
        dependency is architecturally sound but not yet allowed. Ensure
        this new dependency makes architectural sense and get approval from the
        code owners of the `DEPS` file.

## Appendix: Symlinks to enable other projects' presubmits {#presubmit-symlinks}

Creating the following symlinks (POSIX: `ln -s DEST SRC`, Windows:
`mklink /D SRC DEST` from an Admin `cmd` prompt) will get other projects'
presubmit checks working, if you want to upload directly from inside your
Chromium checkout and don't want to use `--bypass-hooks`. All directories assume
you are in your Chromium `src` dir. This list is non-exhaustive; please add to
it as necessary.

* **V8:**
  * Link `v8/buildtools` to `buildtools`
  * Link `v8/third_party/depot_tools` to `depot_tools`
* **WebRTC:**
  * Link `third_party/webrtc/build` to `build`
  * Link `third_party/webrtc/buildtools` to `buildtools`
  * Link `third_party/src` to `.`