# CIPD for chromium dependencies

This document outlines how to use [CIPD][1] for managing binary dependencies in
chromium.

[TOC]

## Adding a new CIPD dependency

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

For more on third-party dependencies, see [here][2].

### 2. Acquire whatever you want to package

Build it, download it, whatever. Once you've done that, lay it out
in your local checkout the way you want it to be laid out in a typical
checkout.

Staying with the example from above, if you want to add a package named
`sample_cipd_dep` that consists of two JARs, `foo.jar` and `bar.jar`, you might
lay them out like so:

```
  third_party/
    sample_cipd_dep/
      ...
      lib/
        bar.jar
        foo.jar
```

### 3. Create a cipd.yaml file

CIPD knows how to create your package based on a .yaml file you provide to it.
The .yaml file should take the following form:

```
# Comments are allowed.

# The package name is required. Third-party chromium dependencies should
# unsurprisingly all be prefixed with chromium/third_party/.
package: chromium/third_party/sample_cipd_dep

# The description is optional and is solely for the reader's benefit. It
# isn't used in creating the CIPD package.
description: A sample CIPD dependency.

# The root is optional and, if unspecified, defaults to ".". It specifies the
# root directory of the files and directories specified below in "data".
#
# You won't typically need to specify this explicitly.
root: "."

# The install mode is optional. If provided, it specifies how CIPD should
# install a package: "copy", which will copy the contents of the package
# to the installation directory; and "symlink", which will create symlinks
# to the contents of the package in the CIPD root inside the installation
# directory.
#
# You won't typically need to specify this explicitly.
install_mode: "symlink"

# The data is required and described what should be included in the CIPD
# package.
data:
  # Data can include directories, files, or a version file.

  - dir: "directory_name"

    # Directories can include an optional "exclude" list of regexes.
    # Files or directories within the given directory that match any of
    # the provided regexes will not be included in the CIPD package.
    exclude:
      - .*\.pyc
      - exclude_me
      - keep_this/but_not_this

  - file: keep_this_file.bin

  # If included, CIPD will dump package version information to this path
  # at package installation.
  - version_file: CIPD_VERSION.json
```

For example, for `sample_cipd_dep`, we might write the following .yaml file:

```
package: chromium/third_party/sample_cipd_dep
description: A sample CIPD dependency.
data:
  - file: bar.jar
  - file: foo.jar
```

For more information about the package definition spec, see [the code][3].

> **Note:** Committing the .yaml file to the repository isn't required,
> but it is recommended. Doing so has benefits for visibility and ease of
> future updates.

### 4. Create your CIPD package

To actually create your package, you'll need:

 - the cipd.yaml file (described above)
 - [permission](#permissions-in-cipd).

Once you have those, you can create your package like so:

```
# Assuming that the third-party dependency in question is at version 1.2.3
# and this is the first chromium revision of that version.
$ cipd auth-login  # One-time auth.
$ cipd create --pkg-def cipd.yaml
...
[P114210 10:14:17.215 client.go:931 I] cipd: instance
chromium/third_party/sample_cipd_dep:TX7HeY1_1JLwFVx-xiETOpT8YK4W5CbyO26SpmaMA0IC was
successfully registered
```

Take note of the instance ID printed in the log
(`TX7HeY1_1JLwFVx-xiETOpT8YK4W5CbyO26SpmaMA0IC` in the example above).
You'll be adding it to DEPS momentarily.

### 5. Add your CIPD package to DEPS

You can add your package to DEPS by adding an entry of the following form to
the `deps` dict:

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

To modify a CIPD dependency, follow steps 2, 3, and 4 above, then modify the
version listed in DEPS.

## Miscellaneous

### Permissions in CIPD

You can check a package's ACLs with `cipd acl-list`:

```
$ cipd acl-list chromium/third_party/sample_cipd_dep
...
```

Permissions in CIPD are handled hierarchically. You can check entries higher
in the package hierarcy with `cipd acl-list`, too:

```
$ cipd acl-list chromium
...
```

By default, [cria/project-chromium-cipd-owners][4] own all CIPD packages
under `chromium/`. If you're adding a package, talk to one of them.

To obtain write access to a new package, ask an owner to run:

```
$ cipd acl-edit chromium/third_party/sample_cipd_dep -owner user:email@address.com
```

## Troubleshooting

 - **A file maintained by CIPD is missing, and gclient sync doesn't recreate it.**

CIPD currently caches installation state. Modifying packages managed by CIPD
will invalidate this cache in a way that CIPD doesn't detect - i.e., CIPD will
assume that anything it installed is still installed, even if you deleted it.
To clear the cache and force a full reinstallation, delete your
`$GCLIENT_ROOT/.cipd` directory.

Note that there is a [bug](https://crbug.com/794764) on file to add a mode to CIPD
that is not so trusting of its own cache.

[1]: https://chromium.googlesource.com/infra/luci/luci-go/+/master/cipd/
[2]: /docs/adding_to_third_party.md
[3]: https://chromium.googlesource.com/infra/luci/luci-go/+/master/cipd/client/cipd/builder/pkgdef.go
[4]: https://chrome-infra-auth.appspot.com/auth/groups/project-chromium-cipd-owners
