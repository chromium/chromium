// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBUI_FLAGS_FLAGS_UI_SWITCHES_H_
#define COMPONENTS_WEBUI_FLAGS_FLAGS_UI_SWITCHES_H_

namespace switches {

extern const char kFlagSwitchesBegin[];
extern const char kFlagSwitchesEnd[];

// Disables all experiments set on about:flags. Does not disable about:flags
// itself. Useful if an experiment makes chrome crash at startup: One can start
// chrome with --no-experiments, disable the problematic lab at about:flags and
// then restart chrome without this switch again.
inline constexpr char kNoExperiments[] = "no-experiments";

}  // namespace switches

#endif  // COMPONENTS_WEBUI_FLAGS_FLAGS_UI_SWITCHES_H_
