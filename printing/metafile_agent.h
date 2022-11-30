// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_AGENT_H_
#define PRINTING_METAFILE_AGENT_H_

#include <string>

#include "base/component_export.h"

namespace printing {

// Inform the printing system that it may embed this user-agent string
// in its output's metadata.
COMPONENT_EXPORT(PRINTING_METAFILE)
void SetAgent(const std::string& user_agent);
COMPONENT_EXPORT(PRINTING_METAFILE) const std::string& GetAgent();

}  // namespace printing

#endif  // PRINTING_METAFILE_AGENT_H_
