# Policy for Adding a New Port

**Before the Chromium project first starts accepting patches for new ports, the
new port/platform must be approved by project leadership.** See the
[contributing guidelines](contributing.md#Code-guidelines) for how to get
approval for new architectures, platforms, or sub-projects.

Since every new port for Chromium has a maintenance cost, here are some
expectations of new ports that the project accepts:

## Expectations

*   Ports should represent a significant ongoing investment to established platforms, rather than hobby or experimental code.
*   These will not have bots on Google-run waterfalls (even FYI).
*   Chromium engineers are not expected to maintain them.
*   As much as possible, try to use existing branches/ifdefs.
*   While changes in src/base are unavoidable, higher level directories shouldn't have to change. i.e. existing porting APIs should be used. We would not accept new rendering pipelines as an example.
