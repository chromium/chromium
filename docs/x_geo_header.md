# X-Geo Geolocation Header Specification for Search Engines

This document describes the `X-Geo` HTTP header sent by Chromium to eligible
Default Search Engine providers.

[TOC]

---

## Overview

When a user selects a search suggestion in Chromium using their Default Search
Engine, Chromium can optionally attach the user's geographical location in the
`X-Geo` HTTP header. This allows the search engine to return localized search
results (e.g. nearby businesses, local weather) on initial navigation without
requiring additional JavaScript Geolocation API usage after page load and a
subsequent reload.

---

## Eligibility and Opt-In Requirements

For this to work, the Default Search Engine (DSE) provider must opt in by being
configured in Chromium's search engine data to receive the header.

Search engine providers wishing to configure their engine definition in Chromium
to receive the `X-Geo` header should follow the instructions in
[//third_party/search_engines_data/README.chromium](https://source.chromium.org/chromium/chromium/src/+/main:third_party/search_engines_data/README.chromium)
to set the `send_x_geo_header` property to `true`.

When the DSE has opted in, Chromium will sometimes (based on various factors and
triggers) send the `X-Geo` header with search requests.

---

## HTTP Header Format

When attached to a search request, the header is formatted as follows:

```http
X-Geo: w <base64url_encoded_protobuf>
```

- **Prefix (`w `)**: The value always begins with the ASCII characters `'w'`
  followed by a space (`"w "`, ASCII `0x77 0x20`). This prefix identifies the
  string as a Base64-encoded Protocol Buffer message conforming to the `X-Geo`
  protocol.
- **Base64Url Encoding**: Following the `"w "` prefix, the binary Protobuf
  payload is encoded using **Base64Url** (RFC 4648 Section 5, URL-safe alphabet:
  `A-Z`, `a-z`, `0-9`, `-`, `_`) with padding characters (`=`).

---

## Protocol Buffer Specification

The binary payload decodes to an `omnibox.LocationDescriptor` Protocol Buffer
message. The `.proto` schema is defined in Chromium at:
[components/omnibox/browser/proto/partner_location_descriptor.proto](https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/proto/partner_location_descriptor.proto)
