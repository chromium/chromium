# CIPD and 3pp for chromium dependencies

[TOC]

## What is CIPD?
* CIPD stands for "Chrome Infrastructure Package Deployment".
* Its code and docs [live within the luci-go project][CIPD].
* Chromium uses CIPD to avoid checking large binary files into git, which git
  does not handle well.
* gclient supports CIPD packages in the same way as git repositories. They are
  specified in [DEPS] and updated via `gclient sync`.
* You can [browse Chromium's CIPD repository][browse] online.

[CIPD]: https://chromium.googlesource.com/infra/luci/luci-go/+/main/cipd/README.md
[DEPS]: /DEPS
[browse]: https://chrome-infra-packages.appspot.com/p/chromium

## What is 3pp?
* 3pp stands for "Third Party Packages" which allows uniform cross-compiliation,
  version tracking and archival for third-party software packages for
  distribution via CIPD.
* The code and docs [live within the recipe module support_3pp][support_3pp].
* By specifying a 3pp package, you can define how to build certain artifacts and
  where to upload to CIPD. Then our packagers will do the rest for you.

[support_3pp]: https://chromium.googlesource.com/infra/infra/+/main/recipes/README.recipes.md#recipe_modules-support_3pp

## Why use CIPD & 3pp?:

* CIPD is our solution to storing large binary blobs directly in git, which git
  is not good at.
* Building these packages with 3pp is beneficial because:
  * It makes how each binary file was created clear and verifiable.
  * It avoids the need for maintaining a list of ACLs for uploading to CIPD.

## Adding a new 3pp package

### 1. Set up a new directory for your dependency

You'll first want somewhere in the repository in which your dependency will
live. For third-party dependencies, this should typically be a subdirectory
of `//third_party`. You'll need to add the same set of things to that
directory that you'd add for a non-CIPD dependency -- OWNERS, README.chromium,
etc.

For example, if you want to add a package named `sample_cipd_dep`, you might
create the following:

```
  third_party/
    sample_cipd_dep/
      LICENSE
      OWNERS
      README.chromium
```

For more on third-party dependencies, see [adding_to_third_party.md].

[adding_to_third_party.md]: /docs/adding_to_third_party.md

### 2. Set up the 3pp subdirectory

The 3pp subdirectory will store all the 3pp related files, including a 3pp spec
(`3pp.pb`), as well as scripts, patches and/or tools to build the software
from source. It should be placed directly under the directory path that matches
the desired name of the cipd package.

Staying with the example from above, the `sample_cipd_dep` directory may be
like the following.

> Note that among the files in 3pp subdirectory, the `3pp.pb` is always
> required. The rest are optional, depending on how the `3pp.pb` is specified.

```
  third_party/
    sample_cipd_dep/
      LICENSE
      OWNERS
      README.chromium
      3pp/
        3pp.pb  # REQUIRED
        bootstrap.py
        fetch.py
        install.sh
        install_win.sh
        patches/
          0001-foo.patch
```

#### 2.1 The file `3pp.pb` (Required)

`3pp.pb` is a text proto specified by the **[`spec.proto`]** schema. It is broken up
into two main sections:
* `create`: allows you to specify how the package software gets created, and
   allows specifying differences in how it's fetched/built/tested on a
   per-target basis. See [here][doc_create] for more details.
* `upload`: contains some details on how the final result gets uploaded to CIPD.
   See [here][doc_upload] for more details.

[`spec.proto`]: https://chromium.googlesource.com/infra/infra/+/main/recipes/recipe_modules/support_3pp/spec.proto
[doc_create]: https://chromium.googlesource.com/infra/infra/+/main/recipes/README.recipes.md#creation-stages
[doc_upload]: https://chromium.googlesource.com/infra/infra/+/main/recipes/README.recipes.md#upload

Staying with the example from above, the file `sample_cipd_dep/3pp/3pp.pb` may
be like the following:

```
create {
  source {
    url {
      download_url: "https://some_url_link/foo.zip"
      version: "1.0.0"
      extension: ".zip"
    }
    patch_version: "cr0"
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "tools"
  universal: true
}
```

While the above example could meet most of the use case, `3pp.pb` is capable of
handling more complicated use case like the following:

```
# create section that is shared by linux-.* and mac-.* platforms
create {
  platform_re: "linux-.*|mac-.*"
  source {
    git {
      repo: "<one_git_repo>"
      tag_pattern: "v%s",

      # Fixed to 3.8.x for now.
      version_restriction: { op: LT val: "3.9a0"}
    }
    patch_dir: "patches"
  }
  build {
    # Can also leave as blank since the script name defaults to "install.sh"
    install: "install.sh"
  }
}

# create section that is specific to linux-.* platforms
create {
  platform_re: "linux-.*"
  build {
    dep: "<dep_foo>"
    dep: "<dep_bar>"

    tool: "<tool_foo>"
  }
}

# create section that is specific to linux-arm.* and linux-mips.* platforms
create {
  platform_re: "linux-arm.*|linux-mips.*"
  build {
    tool: "<tool_bar>"
  }
}

# create section that is specific to windows-*
create {
  platform_re: "windows-.*"
  source { script { name: "fetch.py" } }
  build {
    install: "install_win.sh"
  }
}

upload { pkg_prefix: "tools" }
```

#### 2.2 The file `install.sh` (Optional)

When the `build` message is specified in `3pp.pb`, the file specified in
"build.install" (Default to "install.sh") will be run to transform the source
into the built product.

Staying with the example from above, the file `sample_cipd_dep/3pp/install.sh`
may be like the following.

> Note that during the build stage, the 3pp directory and all its dependent 3pp
> directories (i.e. the `tool` and `dep` from the `build` message in `3pp.pb`)
> will be [copied to a different directory]. So commands in `install.sh` should
> not refer to files that are outside of these directories.

[copied to a different directory]: https://chromium.googlesource.com/infra/infra/+/53fd7d1eda2010009ed00fdc1a7b59fe5034ae0c/recipes/recipe_modules/support_3pp/source.py#246

```
#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# So the commands below should output the built product to this directory.
PREFIX="$1"

# Commands to transform the source into the built product and move it to $PREFIX
./configure
make install

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
cp -a bin_foo "$SCRIPT_DIR/bootstrap.py" "$PREFIX"
```

#### 2.3 The file `fetch.py` (Optional)

When specifying the `source` in 3pp.pb, it is possible to use a custom catch-all
script to probe for the latest version and obtain the latest sources. A simple
example can be like the following:

> Note that this python script should be **python3-compatible**.

```
#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import urllib


def do_latest():
  print(urllib.urlopen('some_url/master/VERSION').read().strip())


def get_download_url(version, platform):
  target_os, target_arch = platform.split('-')
  ext = '.zip' if target_os == 'windows' else '.tar.gz'
  partial_manifest = {
    'url': ['download_url1', 'download_url2'],
    'ext': ext,
  }
  print(json.dumps(partial_manifest))


def main():
  ap = argparse.ArgumentParser()
  sub = ap.add_subparsers()

  latest = sub.add_parser("latest")
  latest.set_defaults(func=lambda _opts: do_latest())

  download = sub.add_parser("get_url")
  download.set_defaults(
    func=lambda _opts: get_download_url(
      os.environ['_3PP_VERSION'], os.environ['_3PP_PLATFORM']
    )
  )

  opts = ap.parse_args()
  opts.func(opts)


if __name__ == '__main__':
  main()
```

### 3. Add "3pp" subdirectory to `chromium/src` repo

#### 3pp CQ builders (Presubmit)

The following are the optional CQ builders to run the presubmit check for CLs
that have the directory "3pp" in the patchset.

* [3pp-linux-amd64-packager](https://ci.chromium.org/p/chromium/builders/try/3pp-linux-amd64-packager):
  For builds on the linux-amd64 platform or universal builds (e.g. to be used by
  all platforms)

#### 3pp CI builders (Postsubmit)

Once the CLs pass the CQ and get landed, the following CI builders will
periodically build all the 3pp packages that match the given platforms and
upload any new results to CIPD.

* [3pp-linux-amd64-packager](https://ci.chromium.org/p/chromium/builders/ci/3pp-linux-amd64-packager):
  For builds on the linux-amd64 platform or universal builds (e.g. to be used by
  all platforms)

### 4. Add your CIPD package to DEPS

Once your CIPD package is created by the 3pp CI builders, you can add it to
`DEPS` by adding an entry of the following form to the `deps` dict:

```
deps = {
  # ...

  # This is the installation directory.
  'src/third_party/sample_cipd_dep': {

    # In this example, we're only installing one package in this location,
    # but installing multiple package in a location is supported.
    'packages': [
      {
        'package': 'chromium/third_party/sample_cipd_dep',
        'version': 'TX7HeY1_1JLwFVx-xiETOpT8YK4W5CbyO26SpmaMA0IC',
      },
    ],

    # As with git-based DEPS entries, 'condition' is optional.
    'condition': 'checkout_android',
    'dep_type': 'cipd',
  },

  # ...
}
```

This will result in CIPD package `chromium/third_party/sample_cipd_dep` at
`TX7HeY1_1JLwFVx-xiETOpT8YK4W5CbyO26SpmaMA0IC` being installed in
`src/third_party/sample_cipd_dep` (relative to the gclient root directory).

## Updating a CIPD dependency

To modify a CIPD dependency, follow steps 2 and 3 above, then modify the
version listed in DEPS.

## Miscellaneous

### Create a cipd.yaml file in the old way

While it is strongly suggested to use 3pp infrastructure, there are existing flows
that create a cipd.yaml file by a GN template or a script, and upload it to CIPD
by builders with custom recipes.

Examples are:
* [android-androidx-packager](https://ci.chromium.org/p/chromium/builders/ci/android-androidx-packager)
* [android-sdk-packager](https://ci.chromium.org/p/chromium/builders/ci/android-sdk-packager)

#### Generating cipd.yaml via GN Template:
The `cipd_package_definition` template in [build/cipd/cipd.gni] can be used to
create the yaml definition as part of Chromium's normal build process. Declare
a target like:
```
cipd_package_definition("my_cipd_package") {
  package = "path/to/cipd/package"
  description = "Prebuilt test binary."
  install_mode = "copy"
  deps = [ "//path/to:test_binary_target" ]
  sources = [ "//path/to:test_binary_file" ]
}
```
[build/cipd/cipd.gni]: https://source.chromium.org/chromium/chromium/src/+/main:build/cipd/cipd.gni

### Permissions in CIPD

You can check a package's ACLs with `cipd acl-list`:

```
$ cipd acl-list chromium/third_party/sample_cipd_dep
...
```

Permissions in CIPD are handled hierarchically. You can check entries higher
in the package hierarchy with `cipd acl-list`, too:

```
$ cipd acl-list chromium
...
```

By default, [cria/project-chromium-cipd-owners][cria] own all CIPD packages
under `chromium/`. If you're adding a package, talk to one of them.

To obtain write access to a new package, ask an owner to run:

```
$ cipd acl-edit chromium/third_party/sample_cipd_dep -owner user:email@address.com
```

[cria]: https://chrome-infra-auth.appspot.com/auth/groups/project-chromium-cipd-owners

## Troubleshooting

 - **A file maintained by CIPD is missing, and gclient sync doesn't recreate it.**

CIPD currently caches installation state. Modifying packages managed by CIPD
will invalidate this cache in a way that CIPD doesn't detect - i.e., CIPD will
assume that anything it installed is still installed, even if you deleted it.
To clear the cache and force a full reinstallation, delete your
`$GCLIENT_ROOT/.cipd` directory.

Note that there is a [bug](https://crbug.com/1176408) on file where
`gclient sync` does not reset CIPD entries that are changed locally.
