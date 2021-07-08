/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_NODE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_helper_mixin.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

// Higher values produce more debugging output.
#define DEBUG_AUDIONODE_REFERENCES 0

namespace blink {

class BaseAudioContext;
class AudioNode;
class AudioNodeOptions;
class AudioNodeInput;
class AudioNodeOutput;
class AudioParam;
class DeferredTaskHandler;
class ExceptionState;

// An AudioNode is the basic building block for handling audio within an
// BaseAudioContext.  It may be an audio source, an intermediate processing
// module, or an audio destination.  Each AudioNode can have inputs and/or
// outputs. An AudioSourceNode has no inputs and a single output.
// An AudioDestinationNode has one input and no outputs and represents the final
// destination to the audio hardware.  Most processing nodes such as filters
// will have one input and one output, although multiple inputs and outputs are
// possible.

// Each of AudioNode objects owns its dedicated AudioHandler object. AudioNode
// is responsible to provide IDL-accessible interface and its lifetime is
// managed by Oilpan GC. AudioHandler is responsible for anything else. We must
// not touch AudioNode objects in an audio rendering thread.

// AudioHandler is created and owned by an AudioNode almost all the time. When
// the AudioNode is about to die, the ownership of its AudioHandler is
// transferred to DeferredTaskHandler, and it does deref the AudioHandler on the
// main thread.
//
// Be careful to avoid reference cycles. If an AudioHandler has a reference
// cycle including the owner AudioNode, objects in the cycle are never
// collected.
class MODULES_EXPORT AudioHandler : public ThreadSafeRefCounted<AudioHandler> {
 public:
  enum NodeType {
    kNodeTypeUnknown = 0,
    kNodeTypeDestination = 1,
    kNodeTypeOscillator = 2,
    kNodeTypeAudioBufferSource = 3,
    kNodeTypeMediaElementAudioSource = 4,
    kNodeTypeMediaStreamAudioDestination = 5,
    kNodeTypeMediaStreamAudioSource = 6,
    kNodeTypeScriptProcessor = 7,
    kNodeTypeBiquadFilter = 8,
    kNodeTypePanner = 9,
    kNodeTypeStereoPanner = 10,
    kNodeTypeConvolver = 11,
    kNodeTypeDelay = 12,
    kNodeTypeGain = 13,
    kNodeTypeChannelSplitter = 14,
    kNodeTypeChannelMerger = 15,
    kNodeTypeAnalyser = 16,
    kNodeTypeDynamicsCompressor = 17,
    kNodeTypeWaveShaper = 18,
    kNodeTypeIIRFilter = 19,
    kNodeTypeConstantSource = 20,
    kNodeTypeAudioWorklet = 21,
    kNodeTypeEnd = 22
  };

  AudioHandler(NodeType, AudioNode&, float sample_rate);
  virtual ~AudioHandler();
  // dispose() is called when the owner AudioNode is about to be
  // destructed. This must be called in the main thread, and while the graph
  // lock is held.
  // Do not release resources used by an audio rendering thread in dispose().
  virtual void Dispose();

  // GetNode() returns a valid object until the AudioNode is collected on the
  // main thread, and nullptr thereafter. We must not call GetNode() in an audio
  // rendering thread.
  AudioNode* GetNode() const;

  // context() returns a valid object until the BaseAudioContext dies, and
  // returns nullptr otherwise.  This always returns a valid object in an audio
  // rendering thread, and inside dispose().  We must not call context() in the
  // destructor.
  virtual BaseAudioContext* Context() const;
  void ClearContext() { context_ = nullptr; }

  DeferredTaskHandler& GetDeferredTaskHandler() const {
    return *deferred_task_handler_;
  }

  enum ChannelCountMode { kMax, kClampedMax, kExplicit };

  NodeType GetNodeType() const { return node_type_; }
  String NodeTypeName() const;

  // This object has been connected to another object. This might have
  // existing connections from others.
  // This function must be called after acquiring a connection reference.
  void MakeConnection();

  // This object will be disconnected from another object. This might have
  // remaining connections from others.  This function must be called before
  // releasing a connection reference.
  //
  // This can be called from main thread or context's audio thread.  It must be
  // called while the context's graph lock is held.
  void BreakConnectionWithLock();

  // The AudioNodeInput(s) (if any) will already have their input data available
  // when process() is called.  Subclasses will take this input data and put the
  // results in the AudioBus(s) of its AudioNodeOutput(s) (if any).
  // Called from context's audio thread.
  virtual void Process(uint32_t frames_to_process) = 0;

  // Like process(), but only causes the automations to process; the
  // normal processing of the node is bypassed.  By default, we assume
  // no AudioParams need to be updated.
  virtual void ProcessOnlyAudioParams(uint32_t frames_to_process) {}

  // No significant resources should be allocated until initialize() is called.
  // Processing may not occur until a node is initialized.
  virtual void Initialize();
  virtual void Uninitialize();

  bool IsInitialized() const { return is_initialized_; }

  unsigned NumberOfInputs() const { return inputs_.size(); }
  unsigned NumberOfOutputs() const { return outputs_.size(); }

  // Number of output channels.  This only matters for ScriptProcessorNodes.
  virtual unsigned NumberOfOutputChannels() const;

  // The argument must be less than numberOfInputs().
  AudioNodeInput& Input(unsigned);
  // The argument must be less than numberOfOutputs().
  AudioNodeOutput& Output(unsigned);
  const AudioNodeOutput& Output(unsigned) const;

  // processIfNecessary() is called by our output(s) when the rendering graph
  // needs this AudioNode to process.  This method ensures that the AudioNode
  // will only process once per rendering time quantum even if it's called
  // repeatedly.  This handles the case of "fanout" where an output is connected
  // to multiple AudioNode inputs.  Called from context's audio thread.
  virtual void ProcessIfNecessary(uint32_t frames_to_process);

  // Called when a new connection has been made to one of our inputs or the
  // connection number of channels has changed.  This potentially gives us
  // enough information to perform a lazy initialization or, if necessary, a
  // re-initialization.  Called from main thread.
  virtual void CheckNumberOfChannelsForInput(AudioNodeInput*);

#if DEBUG_AUDIONODE_REFERENCES
  static void PrintNodeCounts();
#endif
#if DEBUG_AUDIONODE_REFERENCES > 1
  void TailProcessingDebug(const char* debug_note, bool flag);
  void AddTailProcessingDebug();
  void RemoveTailProcessingDebug(bool disable_outputs);
#endif

  // True if the node has a tail time or latency time that requires
  // special tail processing to behave properly.  Ideally, this can be
  // checked using TailTime and LatencyTime, but these aren't
  // available on the main thread, and the tail processing check can
  // happen on the main thread.
  virtual bool RequiresTailProcessing() const = 0;

  // TailTime() is the length of time (not counting latency time) where
  // non-zero output may occur after continuous silent input.
  virtual double TailTime() const = 0;

  // LatencyTime() is the length of time it takes for non-zero output to
  // appear after non-zero input is provided. This only applies to processing
  // delay which is an artifact of the processing algorithm chosen and is
  // *not* part of the intrinsic desired effect. For example, a "delay" effect
  // is expected to delay the signal, and thus would not be considered
  // latency.
  virtual double LatencyTime() const = 0;

  // PropagatesSilence() should return true if the node will generate silent
  // output when given silent input. By default, AudioNode will take TailTime()
  // and LatencyTime() into account when determining whether the node will
  // propagate silence.
  virtual bool PropagatesSilence() const;
  bool InputsAreSilent();
  void SilenceOutputs();
  void UnsilenceOutputs();

  void EnableOutputsIfNecessary();
  void DisableOutputsIfNecessary();
  void DisableOutputs();

  unsigned ChannelCount();
  virtual void SetChannelCount(unsigned, ExceptionState&);

  String GetChannelCountMode();
  virtual void SetChannelCountMode(const String&, ExceptionState&);

  String ChannelInterpretation();
  virtual void SetChannelInterpretation(const String&, ExceptionState&);

  ChannelCountMode InternalChannelCountMode() const {
    return channel_count_mode_;
  }
  AudioBus::ChannelInterpretation InternalChannelInterpretation() const {
    return channel_interpretation_;
  }

  void UpdateChannelCountMode();
  void UpdateChannelInterpretation();

  // Called when this node's outputs may have become connected or disconnected
  // to handle automatic pull nodes.
  virtual void UpdatePullStatusIfNeeded() {}

 protected:
  // Inputs and outputs must be created before the AudioHandler is
  // initialized.
  void AddInput();
  void AddOutput(unsigned number_of_channels);

  // Called by processIfNecessary() to cause all parts of the rendering graph
  // connected to us to process.  Each rendering quantum, the audio data for
  // each of the AudioNode's inputs will be available after this method is
  // called.  Called from context's audio thread.
  virtual void PullInputs(uint32_t frames_to_process);

  // Force all inputs to take any channel interpretation changes into account.
  void UpdateChannelsForInputs();

  // The last time (context time) that his handler ran its Process() method.
  // For each render quantum, we only want to process just once to handle fanout
  // of this handler.
  double last_processing_time_;

  // The last time (context time) when this node did not have silent inputs.
  double last_non_silent_time_;

 private:
  void SetNodeType(NodeType);

  void SendLogMessage(const String& message);

  bool is_initialized_;
  NodeType node_type_;

  // The owner AudioNode. Accessed only on the main thread.
  const WeakPersistent<AudioNode> node_;

  // This untraced member is safe because this is cleared for all of live
  // AudioHandlers when the BaseAudioContext dies.  Do not access m_context
  // directly, use context() instead.
  // See http://crbug.com/404527 for the detail.
  UntracedMember<BaseAudioContext> context_;

  // Legal to access even when |context_| may be gone, such as during the
  // destructor.
  const scoped_refptr<DeferredTaskHandler> deferred_task_handler_;

  Vector<std::unique_ptr<AudioNodeInput>> inputs_;
  Vector<std::unique_ptr<AudioNodeOutput>> outputs_;

  int connection_ref_count_;

  bool is_disabled_;

  // Used to trigger one single textlog indicating that processing started as
  // intended. Set to true once in the first call to the ProcessIfNecessary
  // callback.
  bool is_processing_ = false;

#if DEBUG_AUDIONODE_REFERENCES
  static bool is_node_count_initialized_;
  static int node_count_[kNodeTypeEnd];
#endif

  ChannelCountMode channel_count_mode_;
  AudioBus::ChannelInterpretation channel_interpretation_;

 protected:
  // Set the (internal) channelCountMode and channelInterpretation
  // accordingly. Use this in the node constructors to set the internal state
  // correctly if the node uses values different from the defaults.
  void SetInternalChannelCountMode(ChannelCountMode);
  void SetInternalChannelInterpretation(AudioBus::ChannelInterpretation);

  unsigned channel_count_;
  // The new channel count mode that will be used to set the actual mode in the
  // pre or post rendering phase.
  ChannelCountMode new_channel_count_mode_;
  // The new channel interpretation that will be used to set the actual
  // intepretation in the pre or post rendering phase.
  AudioBus::ChannelInterpretation new_channel_interpretation_;
};

class MODULES_EXPORT AudioNode : public EventTargetWithInlineData,
                                 public InspectorHelperMixin {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(AudioNode, Dispose);

 public:
  ~AudioNode() override;

  void Trace(Visitor*) const override;
  AudioHandler& Handler() const;

  void HandleChannelOptions(const AudioNodeOptions*, ExceptionState&);
  String GetNodeName() const { return Handler().NodeTypeName(); }

  AudioNode* connect(AudioNode*,
                     unsigned output_index,
                     unsigned input_index,
                     ExceptionState&);
  void connect(AudioParam*, unsigned output_index, ExceptionState&);
  void disconnect();
  void disconnect(unsigned output_index, ExceptionState&);
  void disconnect(AudioNode*, ExceptionState&);
  void disconnect(AudioNode*, unsigned output_index, ExceptionState&);
  void disconnect(AudioNode*,
                  unsigned output_index,
                  unsigned input_index,
                  ExceptionState&);
  void disconnect(AudioParam*, ExceptionState&);
  void disconnect(AudioParam*, unsigned output_index, ExceptionState&);
  BaseAudioContext* context() const;
  unsigned numberOfInputs() const;
  unsigned numberOfOutputs() const;
  unsigned channelCount() const;
  void setChannelCount(unsigned, ExceptionState&);
  String channelCountMode() const;
  void setChannelCountMode(const String&, ExceptionState&);
  String channelInterpretation() const;
  void setChannelInterpretation(const String&, ExceptionState&);

  // EventTarget
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final;

  // Called inside AudioHandler constructors.
  void DidAddOutput(unsigned number_of_outputs);

 protected:
  explicit AudioNode(BaseAudioContext&);
  // This should be called in a constructor.
  void SetHandler(scoped_refptr<AudioHandler>);

  // During construction time the handler may not be set properly. Since the
  // garbage collector can call into HasPendingActivity() such calls need to be
  // able to see whether a handle has been set.
  bool ContainsHandler() const;

 private:
  void WarnIfContextClosed() const;
  void Dispose();
  void DisconnectAllFromOutput(unsigned output_index);
  // Returns true if the specified AudioNodeInput was connected.
  bool DisconnectFromOutputIfConnected(unsigned output_index,
                                       AudioNode& destination,
                                       unsigned input_index_of_destination);
  // Returns true if the specified AudioParam was connected.
  bool DisconnectFromOutputIfConnected(unsigned output_index, AudioParam&);

  void SendLogMessage(const String& message);

  Member<BaseAudioContext> context_;

  // Needed in the destructor, where |context_| is not guaranteed to be alive.
  scoped_refptr<DeferredTaskHandler> deferred_task_handler_;

  scoped_refptr<AudioHandler> handler_;

  // Represents audio node graph with Oilpan references. N-th HeapHashSet
  // represents a set of AudioNode objects connected to this AudioNode's N-th
  // output.
  HeapVector<Member<HeapHashSet<Member<AudioNode>>>> connected_nodes_;
  // Represents audio node graph with Oilpan references. N-th HeapHashSet
  // represents a set of AudioParam objects connected to this AudioNode's N-th
  // output.
  HeapVector<Member<HeapHashSet<Member<AudioParam>>>> connected_params_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_NODE_H_
