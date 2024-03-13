# Android WebView renderer extensions

This directory contains APIs exclusive to the Android WebView. Feature authors
cannot begin implementing features under this directory until they follow the
[engagement protocol below](#engagement-protocol).

## Scope of this directory

**This directory is used for**

* Web facing APIs or extensions to web facing APIs that will only be available
  to Android WebView.

**This directory is not used for**

* Android WebView APIs that are Android facing.
* Web APIs going through standardisation.

## Engagement protocol
### Before engagement
When the regular process for [launching
features](https://www.chromium.org/blink/launching-features/) has been found to
be inconclusive, and a reason exists to explore whether an Android WebView
extension would be appropriate, the feature authors:

* **Should** document requirements, and why this would benefit WebView users on
  Android,
* **Should** document explicitly why this functionality cannot be generalised to
  the web platform as a whole,
* **Should not** have committed to the Android WebView extension route,
* **Should not** have sent out an Android WebView extension implementation out
  for review.

### Engagement
Before a decision may be made, the following process should be followed:

1. A review request is sent out to blink-api-owners-discuss@chromium.org
   1. At this stage, feature authors are allowed to begin opening CLs for the
      feature
2. A Blink API owner **must** LGTM the feature on that mailing list before it
   can land within Blink
   1. Feature authors may then land the feature under this directory
3. If any significant changes to the feature happen after step 2, the feature
   author should notify blink-api-owners-discuss@chromium.org, and if a concern
   is raised, go back to step 2.

Feature authors may explore supporting the feature outside of Blink **if** no
Blink API owner LGTMs the feature.
