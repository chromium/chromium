# Image loading experiments in Chrome 116 that impact LCP and CLS

## Increase the priority of the first 5 images

Starting around Chrome 116, we are running an experiment on 50% of Chrome
sessions that increases the priority of the first five images that are not
smaller than 10,000px^2 (100x100) to 'Medium'. Images usually default to
a 'Low' priority unless otherwise specified by
[fetchpriority](https://web.dev/fetch-priority/). The experiment also allows
for two medium-priority resources at a time to be loading while Chrome's
loading scheduler is in "tight mode" (up until the body has been inserted into
the document).

[Relevant bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1431169)

## How does this affect a site's metrics?

The experiment may impact the Largest Contentfuil Paint (LCP) and Cumulative
Layout Shift (CLS) metrics (in most cases for the better). If the LCP element
is an image, it is usually one of the first 5 images in the document and this
experiment may cause it to load sooner (improving LCP). For sites that don't
explicitly specify image layout and dimensions, loading the first five images
sooner can also make it more likely that they are loaded before the first
paint, reducing shifts in the content that come from layout changing when image
dimensions become available.

## When were users affected?

The change was rolled out to 50% of Chrome users around August 11, 2023 which
roughly corresponds with the release of Chrome 116.