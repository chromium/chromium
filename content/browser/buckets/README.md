# Storage Buckets

This folder contains the browser-side implementation of [Storage Buckets](
https://wicg.github.io/storage-buckets/explainer.html). It serves requests from the renderer
(see [third_party/blink/renderer/modules/buckets](https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/buckets/)
by way of the [quota manager](https://source.chromium.org/chromium/chromium/src/+/main:storage/browser/quota/).

See individual class level comments for descriptions
of the various pieces and the [design doc](https://docs.google.com/document/d/1VDm8M4vRyxCpvp_m3Ojs18W9g1WpqvSOsKf0A6hnAFM/edit?usp=sharing) for a high-level overview.
