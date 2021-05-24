# Policy for Adding a New Port

Since every new port for Chromium has a maintenance cost, here are some guidelines for when the project will accept a new port.

## Expectations

*   Ports should represent a significant ongoing investment to established platforms, rather than hobby or experimental code.
*   These will not have bots on Google-run waterfalls (even FYI).
*   Chromium engineers are not expected to maintain them.
*   As much as possible, try to use existing branches/ifdefs.
*   While changes in src/base are unavoidable, higher level directories shouldn't have to change. i.e. existing porting APIs should be used. We would not accept new rendering pipelines as an example.
*   Send an email to [src/OWNERS](https://chromium.googlesource.com/chromium/src/+/main/ENG_REVIEW_OWNERS) for approval.
