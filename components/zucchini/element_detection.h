// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ELEMENT_DETECTION_H_
#define COMPONENTS_ZUCCHINI_ELEMENT_DETECTION_H_

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

class Disassembler;

// Attempts to detect an executable located at start of |image|. If found,
// returns the corresponding disassembler. Otherwise returns null.
std::unique_ptr<Disassembler> MakeDisassemblerWithoutFallback(
    ConstBufferView image);

// Attempts to create a disassembler corresponding to |exe_type| and initialize
// it with |image|, On failure, returns null.
std::unique_ptr<Disassembler> MakeDisassemblerOfType(ConstBufferView image,
                                                     ExecutableType exe_type);

// Returns the version associated with disassembler of type |exe_type|.
uint16_t DisassemblerVersionOfType(ExecutableType exe_type);

// Attempts to detect an element associated with |image| and returns it, or
// returns nullopt if no element is detected.
using ElementDetector =
    base::RepeatingCallback<std::optional<Element>(ConstBufferView image)>;

// Implementation of ElementDetector using disassemblers.
std::optional<Element> DetectElementFromDisassembler(ConstBufferView image);

// A class to scan through an image and iteratively detect elements.
class ElementFinder {
 public:
  ElementFinder(ConstBufferView image, ElementDetector&& detector);
  ElementFinder(const ElementFinder&) = delete;
  const ElementFinder& operator=(const ElementFinder&) = delete;
  ~ElementFinder();

  // Scans for the next executable using |detector|. Returns the next element
  // found, or nullopt if no more element can be found.
  std::optional<Element> GetNext();

 private:
  ConstBufferView image_;
  ElementDetector detector_;
  offset_t pos_ = 0;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ELEMENT_DETECTION_H_
