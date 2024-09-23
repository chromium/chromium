# Managing Chromium dependencies

Chromium uses `gclient` (part of depot_tools) to manage dependencies (e.g. V8,
WebRTC). Information such as URLs and hashes is stored in the `DEPS` file
located in the root of the project. In addition to `DEPS`, `gclient` may read
git submodules (see
[depot_tools submodules support](https://docs.google.com/document/d/1N_fseFNOj10ETZG3pZ-I30R__w96rYNtvx5y_jFGJWw/view)).

`gclient` supports three dependency types: git, [gcs](gcs_dependencies.md), and
[cipd](cipd_and_3pp.md).

[TOC]

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
