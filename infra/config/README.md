**IMPORTANT:** This branch only has an effect for branches that have projects
set up in
https://chrome-internal.googlesource.com/infradata/config/+/HEAD/configs/luci-config/projects.cfg

This directory contains chromium project-wide configurations
for Chrome Operations services.
For example, [cr-buildbucket.cfg](generated/luci/cr-buildbucket.cfg) defines
builders.

Currently active version can be checked at
https://luci-config.appspot.com/#/projects/chromium .

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

* lib - Utilities for defining LUCI entities.
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
