// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_PUBLIC_TEST_SETUP_FIELD_TRIALS_H_
#define CONTENT_PUBLIC_TEST_SETUP_FIELD_TRIALS_H_

namespace content {
// Some features are shipped using field trials (Finch). They are disabled by
// default, and enabled remotely for a fraction of users.
//
// Features must be sufficiently tested before reaching Dev/Beta/Stable users.
// Tooling mandates that Chrome engineers list features used in a JSON file:
// testing/variations/fieldtrial_testing_config.json.
//
// To ensure proper test coverage, unbranded builds the field trials
// testing config enabled, while branded builds have it disabled.
//
// This function sets up field trials for tests. This is meant to be used in the
// layers at the level or below `content/`. Content embedders aren't meant to
// call this function, because they should have their more idiomatic way of
// setting up their field trials.
//
// TODO(https://crbug.com/40105939): Use it in `blink_unittests`.
void SetupFieldTrials();
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SETUP_FIELD_TRIALS_H_
