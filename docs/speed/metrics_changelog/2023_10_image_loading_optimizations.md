# Image loading optimizations in Chrome 118 that impact LCP and CLS

We are launching two complementary changes for image loading optimizations:
- [Increase the priority of the first 5 images](#increase-the-priority-of-the-first-5-images)
- [Prioritize image load tasks](#prioritize-image-load-tasks)

## Increase the priority of the first 5 images

Starting around Chrome 118, we are launching the [change](2023_08_image_loading.md)
to 100% of Chrome sessions.

## Prioritize image load tasks

When we have an image load task and a rendering task to choose from, we choose
the image load task first to prevent layout shift of an intermediate frame
that doesn't have the image. For the same reason, LCP improves because we skip
the intermediate frame.

[Relevant bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1416030)

## How does this affect a site's metrics?

As we are increasing the priority of the first 5 images and image loading tasks,
sites with images might see better CLS and LCP scores.

## When were users affected?

Both changes were rolled out to 100% of Chrome users around October 1, 2023 which
roughly corresponds with the release of Chrome 118.