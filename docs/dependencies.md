# Managing Chromium dependencies

Chromium uses gclient (part of depot_tools) to manage dependencies. Information
is stored in DEPS file located in the root of the project. In addition to DEPS
file, gclient may read git submodules (see
[depot_tools submodules support](https://docs.google.com/document/d/1N_fseFNOj10ETZG3pZ-I30R__w96rYNtvx5y_jFGJWw/view)).

gclient supports two dependency types: git and [cipd](cipd_and_3pp.md).

[TOC]

## Adding dependencies

Add your entry in DEPS file. Then, run `gclient gitmodules` to generate
git submodules (it will contain .gitmodule change, and gitlink). Edit OWNERS
file and add gitlink path. For example, if new dependency is "src/foo/bar.git",
its gitlink path is "foo/bar". You can confirm that by running `git status`.

Then, run `git add DEPS OWNERS` to stage those files for commit, followed by
`git commit`. Your change is now ready to be sent for a review using `git cl
upload`.

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

Example of DEPS file:
```
vars = {
  'boringssl_git': 'https://boringssl.googlesource.com',
  'boringssl_revision': 'e4acd6cb568214b1c7db4e59ce54ea2e1deae1f5',
}
deps = {
  'src/third_party/boringssl/src':
    Var('boringssl_git') + '/boringssl.git' + '@' +  Var('boringssl_revision'),
}
```

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
by manually editing DEPS file. If git submodules are used, you will also need to
remove it from git:

```
git rm --cache {dependency}
```

or using the example from the previous section:

```
git rm --cache third_party/boringssl/src
```

Once git-rm is executed, you can proceed with committing your change. You should
see change in DEPS, .gitmodule and {dependency}.

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
