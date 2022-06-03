# Developing Chrome for iOS with the Xcode IDE

This document contains notes used during a presentation about how to use Xcode
to develop Chrome for iOS.

## Background

### .xcodeproj

*   Open all.xcodeproj only.
*   Generated from BUILD.gn files.
*   Don't make changes to it as it is a "fake" representation of the project
    -   changes will not be committed
    -   changes will be overwritten without warning
*   Modify BUILD files instead
*   Added BUILD.gn files/`source_sets` need to be referenced from other ones as
    a `dep` in order for it to be shown in xcode.

### Adding new files

*   Create new files with `tools/boilerplate.py`
*   Add files to BUILD.gn `mate ios/chrome/...BUILD.gn`
*   Add new source_set target as a dep of another target
*   Execute `gclient runhooks` or `gclient sync`

### Simulators

*   Simulators build for Mac architecture, not emulators. Fast, generally good
    for most development
*   To run on device, need provisioning profiles and certificate

## Xcode

*   Project location is `src/out/build/`, open `all.xcodeproj`
*   Choose "Automatically generate targets" after a `gclient sync`
*   Start typing while dropdowns are presented to filter the displayed items

### Targets

*   `chrome' is the main application target
*   `ios_web_shell` and `ios_web_view_shell` test lower level of application

### Tests

*   Unittests/Inttests
    -   Add flags to specify tests to run (Option-Click on unittest target name,
        select Run from left panel, add to "Arguments Passed on Launch")
    -   `gtest_filter=TestClass.*`
    -   `gtest_repeat=20`
    -   Full list of options available by sending an unknown option

*   EarlGrey
    -   EG1 deprecated
    -   EG2 are current UI tests
        -   A separate "test" process communicates with "the app".
        -   EDO is term for "distant object" on "the other side".
        -   egtest files can run under both EG1 and EG2 with correct setup in
            BUILD.gn because the layer of helpers encapsulate the necessary
            differences.
