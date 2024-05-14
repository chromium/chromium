// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_node_wiring.h"

#include "base/memory/raw_ref.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

namespace {

using AudioNodeOutputSet = HashSet<AudioNodeOutput*>;

struct FindOutputResult {
  const raw_ref<AudioNodeOutputSet> output_set;
  AudioNodeOutputSet::const_iterator iterator;
  bool is_disabled;
};

// Given a connected output, finds it in the "active" or "disabled" set (e.g.
// of the outputs connected to an input). Produces the set in which it was
// found, an iterator into that set (so that it can be erased), and whether or
// not the set it was found in was the disabled set.
//
// It is an error to pass an output which is *not* connected (i.e. is neither
// active nor disabled).
FindOutputResult FindOutput(AudioNodeOutput& output,
                            AudioNodeOutputSet& outputs,
                            AudioNodeOutputSet& disabled_outputs) {
  auto it = outputs.find(&output);
  if (it != outputs.end()) {
    return {raw_ref(outputs), it, false};
  }

  it = disabled_outputs.find(&output);
  if (it != disabled_outputs.end()) {
    return {raw_ref(disabled_outputs), it, true};
  }

  NOTREACHED_IN_MIGRATION() << "The output must be connected to the input.";
  return {raw_ref(outputs), {}, false};
}

}  // namespace

void AudioNodeWiring::Connect(AudioNodeOutput& output, AudioNodeInput& input) {
  input.GetDeferredTaskHandler().AssertGraphOwner();

  const bool input_connected_to_output =
      input.outputs_.Contains(&output) ||
      input.disabled_outputs_.Contains(&output);
  const bool output_connected_to_input = output.inputs_.Contains(&input);
  DCHECK_EQ(input_connected_to_output, output_connected_to_input);

  // Do nothing if already connected.
  if (input_connected_to_output) {
    return;
  }

  (output.is_enabled_ ? input.outputs_ : input.disabled_outputs_)
      .insert(&output);
  output.inputs_.insert(&input);

  // If it has gained an active connection, the input may need to have its
  // rendering state updated.
  if (output.is_enabled_) {
    input.ChangedOutputs();
  }

  // The input node's handler needs to know about this connection. This may
  // cause it to re-enable itself.
  input.Handler().MakeConnection();
}

void AudioNodeWiring::Connect(AudioNodeOutput& output,
                              AudioParamHandler& param) {
  param.GetDeferredTaskHandler().AssertGraphOwner();

  const bool param_connected_to_output = param.outputs_.Contains(&output);
  const bool output_connected_to_param = output.params_.Contains(&param);
  DCHECK_EQ(param_connected_to_output, output_connected_to_param);

  // Do nothing if already connected.
  if (param_connected_to_output) {
    return;
  }

  param.outputs_.insert(&output);
  output.params_.insert(&param);

  // The param may need to have its rendering state updated.
  param.ChangedOutputs();
}

void AudioNodeWiring::Disconnect(AudioNodeOutput& output,
                                 AudioNodeInput& input) {
  input.GetDeferredTaskHandler().AssertGraphOwner();

  // These must be connected.
  DCHECK(output.inputs_.Contains(&input));
  DCHECK(input.outputs_.Contains(&output) ||
         input.disabled_outputs_.Contains(&output));

  // Find the output in the appropriate place.
  auto result = FindOutput(output, input.outputs_, input.disabled_outputs_);

  // Erase the pointers from both sets.
  result.output_set->erase(result.iterator);
  output.inputs_.erase(&input);

  // If an active connection was disconnected, the input may need to have its
  // rendering state updated.
  if (!result.is_disabled) {
    input.ChangedOutputs();
  }

  // The input node's handler may try to disable itself if this was the last
  // connection. This must happen after the set erasures above, or the disabling
  // logic would observe an inconsistent state.
  input.Handler().BreakConnectionWithLock();
}

void AudioNodeWiring::Disconnect(AudioNodeOutput& output,
                                 AudioParamHandler& param) {
  param.GetDeferredTaskHandler().AssertGraphOwner();

  DCHECK(param.outputs_.Contains(&output));
  DCHECK(output.params_.Contains(&param));

  // Erase the pointers from both sets.
  param.outputs_.erase(&output);
  output.params_.erase(&param);

  // The param may need to have its rendering state updated.
  param.ChangedOutputs();
}

void AudioNodeWiring::Disable(AudioNodeOutput& output, AudioNodeInput& input) {
  input.GetDeferredTaskHandler().AssertGraphOwner();

  // These must be connected.
  DCHECK(output.inputs_.Contains(&input));
  DCHECK(input.outputs_.Contains(&output) ||
         input.disabled_outputs_.Contains(&output));

  // The output should have been marked as disabled.
  DCHECK(!output.is_enabled_);

  // Move from the active list to the disabled list.
  // Do nothing if this is the current state.
  if (!input.disabled_outputs_.insert(&output).is_new_entry) {
    return;
  }
  input.outputs_.erase(&output);

  // Since it has lost an active connection, the input may need to have its
  // rendering state updated.
  input.ChangedOutputs();

  // Propagate disabled state downstream. This must happen after the set
  // manipulations above, or the disabling logic could observe an inconsistent
  // state.
  input.Handler().DisableOutputsIfNecessary();
}

void AudioNodeWiring::Enable(AudioNodeOutput& output, AudioNodeInput& input) {
  input.GetDeferredTaskHandler().AssertGraphOwner();

  // These must be connected.
  DCHECK(output.inputs_.Contains(&input));
  DCHECK(input.outputs_.Contains(&output) ||
         input.disabled_outputs_.Contains(&output));

  // The output should have been marked as enabled.
  DCHECK(output.is_enabled_);

  // Move from the disabled list to the active list.
  // Do nothing if this is the current state.
  if (!input.outputs_.insert(&output).is_new_entry) {
    return;
  }
  input.disabled_outputs_.erase(&output);

  // Since it has gained an active connection, the input may need to have its
  // rendering state updated.
  input.ChangedOutputs();

  // Propagate enabled state downstream. This must happen after the set
  // manipulations above, or the disabling logic could observe an inconsistent
  // state.
  input.Handler().EnableOutputsIfNecessary();
}

bool AudioNodeWiring::IsConnected(AudioNodeOutput& output,
                                  AudioNodeInput& input) {
  input.GetDeferredTaskHandler().AssertGraphOwner();

  bool is_connected = output.inputs_.Contains(&input);
  DCHECK_EQ(is_connected, input.outputs_.Contains(&output) ||
                              input.disabled_outputs_.Contains(&output));
  return is_connected;
}

bool AudioNodeWiring::IsConnected(AudioNodeOutput& output,
                                  AudioParamHandler& param) {
  param.GetDeferredTaskHandler().AssertGraphOwner();

  bool is_connected = output.params_.Contains(&param);
  DCHECK_EQ(is_connected, param.outputs_.Contains(&output));
  return is_connected;
}

void AudioNodeWiring::WillBeDestroyed(AudioNodeInput& input) {
  // This is more or less a streamlined version of calling Disconnect
  // repeatedly. In particular it cannot happen while the input's handler is
  // being destroyed, and so does not require any information about these final
  // changes to its connections.
  //
  // What does matter, however, is ensuring that no AudioNodeOutput holds a
  // dangling pointer to `input`.

  input.GetDeferredTaskHandler().AssertGraphOwner();

  for (AudioNodeOutput* output : input.outputs_) {
    output->inputs_.erase(&input);
  }
  for (AudioNodeOutput* output : input.disabled_outputs_) {
    output->inputs_.erase(&input);
  }
  input.outputs_.clear();
  input.disabled_outputs_.clear();
}

}  // namespace blink
