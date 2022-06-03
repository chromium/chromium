This directory contains (most of) the implementation of the [Web App Manifest
specification](https://www.w3.org/TR/appmanifest/).

The internal representation of the manifest, as returned by the manifest parser,
is defined as a Mojo struct in
[blink/public/mojom/manifest/manifest.mojom](third_party/blink/public/mojom/manifest/manifest.mojom).
The [manifest parser](manifest_parser.cc) is responsible for converting the
manifest from its JSON representation to that Mojo struct.

The interpretation of those manifest members is typically done elsewhere,
usually in the browser layer (not in Blink), since most of the Manifest features
describe how the browser interacts with the operating system.
