# AudioNode Tail Processing

The WebAudio API has a concept of a
[tail-time](https://webaudio.github.io/web-audio-api/#tail-time) in
which an AudioNode must continue to process even if the input(s) to
the node are disconnected. For example, if you have a `DelayNode` with
a delay of 1 sec, the node must continue to output data for at least 1
sec to flush out all the delayed data.

# Implementation Details

## Basic Concepts

To implement this, we introduce the `TailTime` and `LatencyTime`
methods for each node.  This isn't required in the spec, but the
implementation makes this distinction. (Not sure why).  TailTime is
how long it takes before a node that has silent input will produce
silent output.  LatencyTime is how long it takes non-silent input to
produce non-silent output.

The sum of these two terms tells us how long a node needs to process
to flush out its internal state.

## Details

To support tail processing several routines are used:

- `PropagatesSilence`: returns true if the node is producing silence
after knowing the inputs are silent

- `RequiresTailProcessing: returns true if the node needs tail
processing.  For example, `GainNode`s have no memory so tail
processing is not needed.

## Triggering Tail Processing

### Silent Inputs

The actual details are a bit complicated, but basically
`AudioNode::ProcessIfNecessary` manages this.  It keeps track of when
the node was last non-silent.  If the inputs are silent and
`PropagatesSilence` returns false, the nodes `Process` method is still
called.

For most nodes, `PropagatesSilence` checks to see if the last
non-silent time plus the `TailTime` plus the `LatencyTime` is greater
than the context `currentTime`. If so, then the node produces silence
now because the internal state has been flushed out.

### Disabled Outputs

There is another way to start tail processing, and this is the more
difficult case that we need to handle.  Consider an `OscillatorNode`
connected to a `DelayNode`.  When the `OscillatorNode` stops, it
disables its output, basically marking its output as silent.  This
normally propagates down the graph disabling the output of each node.

However, the `DelayNode` needs to continue processing.  The logic for
tail processing is in `AudioNode::DisableOutputsIfNecessary`.  If
`RequiresTailProcessing()` returns true, this node is added to the
tail processing handler list (`tail_processing_handlers_`) via
`DeferredTaskHandler::AddTailProcessingHandler()`.  If not, the output
of the node is disabled which propagates through the downstream
nodes.

Then during the beginning and end of each render quantum, we check the
tail processing list to see if the handler would be silent (via
`PropagatesSilence()`).  If it would produce silence, it's removed from
the list.  Otherwise, nothing is done.

Note also that if a connection is made to the node, it is removed from
the tail processing list since it's not processing the tail anymore.


### Some Complications

Because WebAudio has two threads: the main thread and the audio
rendering thread, things are a bit more complicated.  Removing a
handler from tail processing cannot be done on the audio thread
because it requires changing the state of the output.  Thus, when this
happens, the handler is removed from the tail processing list and
placed on the `finished_tail_processing_handlers_` list.  At Each
render quantum, a task is posted to the main thread to update the
output state.

### Additional Complications

In addition, we had to make some guesses on the tail time for
`BiquadFilterNodes` and `IIRFilterNodes`.  In theory, these have an
infinite tail since the both of thse are infinite impulse response
filters.  In practice, we don't really want the `TailTime` to be
infinite because then the nodes basically never go away.

For an `IIRFilterNode`, we actually compute the impulse response and
determine approximately where the impulse response is low enough to
say it is done.  We also arbirarily limit the maximum value, just to
prevent huge tail times.

For a `BiquadFilterNode`, we don't compute the impulse response
because automations of the filter parameters can change the tail.
Instead, we determine the poles of the filter and from that determine
roughly the analytical impulse response.  From the response, we
determine when the response is low enough to say the tail is done.
Like the `IIRFIlterNode`, this is limited to a max value.

For a `DelayNode`, we just set the tail time to be the max delay value
for the node instead of trying to determine a tail time from the
actual `delayTime` parameter.


# Summary

## Important Variables

- `DeferredTaskHandler::tail_processing_handlers_`
- `DeferredTaskHandler::finished_tail_processing_handlers_`

## Important Methods

- `AudioNodeHandler::ProcessIfNecessary`
- `AudioNodeHandler::EnableOutputsIfNecessary`
- `AudioNodeHandler::TailTime`
- `AudioNodeHandler::LatencyTime`
- `AudioNodeHandler::PropagatesSilence`
- `AudioNodeHandler::RequiresTailProcessing`
- `DeferredTaskHandler::AddTailProcessingHandler`
- `DeferredTaskHandler::RemoveTailProcessingHandler`
- `DeferredTaskHandler::UpdateTailProcessingHandlers`
