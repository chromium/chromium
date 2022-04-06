/*
 * Copyright (C) 2012, Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BASIC_INSPECTOR_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BASIC_INSPECTOR_NODE_H_

#include "third_party/blink/renderer/modules/webaudio/audio_node.h"

namespace blink {

class BaseAudioContext;

// AudioBasicInspectorNode is an AudioNode with one input and possibly one
// output where the output might not necessarily connect to another node's
// input.  (It is up to the subclasses to create the output, if needed.)  If the
// output is not connected to any other node, then the AudioBasicInspectorNode's
// processIfNecessary() function will be called automatically by
// BaseAudioContext before the end of each render quantum so that it can inspect
// the audio stream.
class AudioBasicInspectorNode : public AudioNode {
 protected:
  explicit AudioBasicInspectorNode(BaseAudioContext& context);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BASIC_INSPECTOR_NODE_H_
