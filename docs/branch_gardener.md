# Chromium Branch Gardening

The Chrome release branch gardener provides coverage for release branches
(stable and beta) under Pacific timezone shifts.

The goals of a branch gardener are quite similar to those of a trunk gardener.
Branch gardeners need to ensure that:

1.  **Compile failures get fixed**, because compile failures on branches block
    all tests (both automated and manual) and consequently reduce our confidence
    in the quality of what we're shipping, possibly to the point of blocking
    releases.
2.  **Consistent test failures get repaired**, because they similarly reduce our
    confidence in the quality of our code.

For more information on Chromium Branch Gardeners, including How Tos, Swapping
Shifts and rotation updates, please see [Chromium
Branch Gardening](http://goto.google.com/chrome-branch-gardening)
