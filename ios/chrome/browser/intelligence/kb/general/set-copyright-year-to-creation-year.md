---
name: ios/set-copyright-year-to-creation-year
description: Include the standard three-line Chromium BSD copyright header with the creation year on all new source and build files, and preserve the original copyright year when moving, splitting, or editing existing files
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/c++.md
    heading: File headers
    quote: 'All files in Chromium start with a common license header. That header should look like this: ```c++ // Copyright $YEAR The Chromium Authors // Use of this source code is governed by a BSD-style license that can be // found in the LICENSE file. ``` Some important notes about this header: * `$YEAR` should be set to the current year at the time a file is created, and not changed thereafter.'
  - path: docs/imported/gn/style_guide.md
    heading: Example
    quote: '# Copyright 2016 The Chromium Authors. All rights reserved. # Use of this source code is governed by a BSD-style license that can be # found in the LICENSE file.'
---

**Rule.** Start every new file (source `.h`/`.mm`/`.cc`, GN `BUILD.gn` and
`*.gni`, Python, and other text files) with the standard three-line Chromium BSD
copyright header in the file's comment syntax (`//` for C++/Objective-C++, `#`
for GN and Python): `Copyright <year> The Chromium Authors`,
`Use of this source code is governed by a BSD-style license that can be`,
`found in the LICENSE file.`. When creating a new file (including files split or
extracted from existing files), set `<year>` to the calendar year in which the
file is created; never omit the header, copy an older year from the file it was
split from, or bypass presubmit license checks with `Bypass-Check-License`. When
moving, renaming, refactoring, or updating an existing file, retain the original
creation year in the header and do not alter or bump it.

**Why.** All Chromium repository files must begin with the canonical three-line
BSD copyright header to establish legal ownership and creation provenance.
Automated presubmit tools and license scanners rely on this standard format.
Omitting the header or copying an outdated year onto newly created or split
files triggers presubmit failures that must not be masked with
`Bypass-Check-License`, while bumping the year on moved, renamed, or updated
files violates style guidelines and causes unnecessary churn.

## Examples

Bad:

```cpp
# --- Variant 1: Omitting the copyright header entirely on a new GN file ---
import("//build/config/ios/rules.gni")

source_set("connectors_internals") {
  sources = [ "connectors_internals_page_handler.mm" ]
}

# --- Variant 2: File created in 2026, split out of a 2024 file, keeping the
# --- older year (or masking the presubmit with 'Bypass-Check-License') ---
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

// --- Variant 3: File created in 2024, moved in 2026, with its year bumped ---
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
```

Good:

```cpp
# --- Variant 1: New GN file created in 2026 carries 2026 ---
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import("//build/config/ios/rules.gni")

source_set("connectors_internals") {
  sources = [ "connectors_internals_page_handler.mm" ]
}

// --- Variant 2: New .mm file created in 2026 carries 2026 ---
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// --- Variant 3: File created in 2024 keeps 2024 when moved or updated ---
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
```
