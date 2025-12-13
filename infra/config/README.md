# Configurations for Chrome Operation services

**IMPORTANT:** This branch only has an effect for branches that have projects
set up in
https://chrome-internal.googlesource.com/infradata/config/+/HEAD/configs/luci-config/projects.cfg

This directory contains chromium project-wide configurations
for Chrome Operations services.
For example, [cr-buildbucket.cfg](generated/luci/cr-buildbucket.cfg) defines
builders.

Currently active version can be checked at
https://luci-config.appspot.com/#/projects/chromium .

## Starlark configs

The configuration is written using starlark, which is executed using lucicfg to
generate the raw cfg files located in [generated](generated). See
https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md
for more information on lucicfg/starlark.

The starlark configuration is rooted in `main.star` and `dev.star`, which
execute other starlark files to generate a subset of the LUCI service
configuration files. A presubmit check enforces that the generated files are
kept in sync with the output of the starlark configuration.

The configuration rooted at [main.star](main.star) defines the LUCI services
configuration for the chromium project on the production instance of LUCI. The
configuration is responsible for generating the raw configuration files that do
not end in -dev.cfg as well as the markdown file
[cq-builders.md](generated/cq-builders.md). Starlark files in the following
directories are consumed by the configuration:

* lib - Mostly lists of hard-coded constants.
* targets - A local lucicfg package containing definitions of chromium targets
  and tests that will be executed by builders. See
  [its README.md](targets/README.md) for information on how to use the package
  in other projects.
* subprojects - Definitions of LUCI entities.
* generators - Definitions of lucicfg generators that do various things to
  post-process the LUCI configuration before the output files are generated.
  (e.g. generate no-op jobs to workaround limitations of our recipe config) or
  generate additional files (e.g. the CQ builders markdown document).
* validators - Definitions of lucicfg generators that perform additional
  validation on the the LUCI configuration (e.g. ensure all builders are added
  to at least one console).
* outages - Definitions of config settings for operations common when handling
  outages.

The configuration rooted at [dev.star](dev.star) defines the LUCI services
configuration for the chromium project on the dev instance of LUCI. This
configuration consumes starlark files under the dev directory and is responsible
for generating the raw configuration files ending in -dev.cfg.

## Starlark libs

Much of the starlark configuration here uses libraries defined in a separate
[infra/chromium.git][infra-chromium] repo. These libraries traverse and manipulate
lucicfg's [internal graph][lucicfg-graph] to represent components of Chromium's
infra (e.g. nodes for things like builders and tests, and links between related
elements). This code is in a separate repo so it can be shared and reused
elsewhere, mainly src-internal.

Normally, when generating the LUCI configs here, the
[infra/chromium.git][infra-chromium] repo is fetched on-demand using lucicfg's
[package system][lucicfg-package]. However, if you have that repo checked out
locally and want to re-generate the configs here using it instead, you can do so
via:
```
lucicfg generate -repo-override chromium.googlesource.com/infra/chromium/+/refs/heads/main=/path/to/local/infra/chromium/checkout main.star
```

## VS Code

In order to get syntax highlighting, the [Bazel][bazel-extension] can be installed.

In order to get "go to definition" to work, an extra language server needs to be
available for the [Bazel][bazel-extension] extension. This currently requires
applying a custom patch on top of the [bazel-lsp][bazel-lsp] project and
compiling the language server binary, then using it via settings.json in VS
Code. See the [//tools/vscode/bazel_lsp/README.md][bazel-lsp-readme] for more
details.

[bazel-extension]: https://marketplace.visualstudio.com/items?itemName=BazelBuild.vscode-bazel
[bazel-lsp]: https://github.com/cameron-martin/bazel-lsp
[bazel-lsp-readme]: ../../tools/vscode/bazel_lsp/README.md
[infra-chromium]: https://chromium.googlesource.com/infra/chromium
[lucicfg-graph]: https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/lucicfg/doc/README.md#rules_state-representation
[lucicfg-package]: https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/lucicfg/doc/README.md#modules-and-packages
