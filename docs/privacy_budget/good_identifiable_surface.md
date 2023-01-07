# What's a Good Candidate For an IdentifiableSurface? {#good-surface}

Once you have a source of potentially identifying information picked out, you
need to determine how to represent the surface using
[`blink::IdentifiableSurface`].

The first step would be to determine what the surface *is* in the first place.

_If the surface were to be presented as a question that the document asks
a user-agent, what details should the question include in order for the answer
to be identifiable across a wide range of user-agents?_

Sometimes the question is straightforward. E.g. [`window.screenTop`] pretty much
captures the entire question. But it can get tricky as we'll see in the
examples below.

All the pieces of information that the document needs to present to the
user-agent in order to ask the identifiable question should be represented in
the [`blink::IdentifiableSurface`].

There are two broad categories of identifiable surfaces:

*  **Direct Surfaces**: Surfaces accessible via looking up an attribute or
   invoking a parameter-less operation of a global singleton object.

   *Global singleton objects* refer to an object which is effectively the only
   instance of its interface in a single execution context. If one were to
   start from the global object and follow a chain of attributes or
   parameter-less methods, all objects encountered along the way are global
   singleton objects. One could pluck the attribute or operation out of the
   last interface and stick it in the global object and there would be no
   semantic difference.

   In our `window.screenTop` example the global object exposes the `Window`
   interface. `Window.window` or just `window` is a reference back to this
   global object. The `Window` interface exposes the `screenTop` attribute. So
   `window.screenTop` is an expression that looks up an attribute of the global
   object.

   By convention direct surfaces are represented using their corresponding
   [`blink::WebFeature`]. All APIs that are direct identifiable surfaces should
   have corresponding [Use Counters] and hence corresponding `WebFeature`
   values.

   For `window.screenTop`, the resulting `IdentifiableSurface` constructor
   would look like this:

   ```cpp
   IdentifiableSurface::FromTypeAndToken(
       Type::kWebFeature,                  // All direct surfaces use this type.
       WebFeature::WindowScreenTop)
   ```

   See [Instrumenting Direct Surfaces] for details on how to instrument
   these surfaces.

2. **Indirect Surfaces**. A.k.a. everything else.

   [`HTMLMediaElement.canPlayType()`] takes a string indicating a MIME type and
   returns a vague indication of whether that media type is supported (its
   return values are one of `"probably"`, `"maybe"`, or `""`). So the question
   necessarily must include the MIME type.

   Hence the `IdentifiableSurface` constructor could look like this:

   ```cpp
   IdentifiableSurface::FromTypeAndToken(
       Type::kHTMLMediaElement_CanPlayType,
       IdentifiabilityBenignStringToken(mime_type))
   ```

   The [`blink::IdentifiableSurface`] includes:
   * That it represents an `HTMLMediaElement.canPlayType()` invocation.
     `Type::kHTMLMediaElement_CanPlayType` is a bespoke enum value that was
     introduced for this specific surface. See [Adding a Surface Type]
     for details on how to add a surface type.
   * The parameter that was passed to the operation.

   `IdentifiabilityBenignStringToken` is a helper function that calculates
   a digest of a "benign" string. See [Instrumentation] for more details on how
   to represent operation arguments.

The distinction between a direct surface and an indirect surface can sometimes
be fuzzy. But it's always based on what's known _a priori_ and what's practical
to measure. A `canPlayType("audio/ogg; codecs=vorbis")` query could just as
easily be represented as a `WebFeature` like
`MediaElementCanPlayType_Audio_Ogg_Codecs_Vorbis`. But

* [This doesn't scale].
* The set of MIME types can be pretty large and changing.
* It's not possible to hardcode all possible values at coding time.
* Most of the values will be irrelevant to identifiability, but we don't know which ones.

All things considered, deriving a digest for the argument is much more
practical than alternatives.

### Example: NetworkInformation.saveData {#eg-net-effective-type}

The following expression yields whether the user-agent is operating under
a reduce data usage constraint (See [`NetworkInformation.saveData`]):

```js
navigator.connection.saveData
```

This is a [direct surface]. As such constructing a `IdentifableSurface` only
requires knowing the interface and name of the final attribute or operation in
the expression.

Hence the `IdentifiableSurface` is of type `kWebFeature` with a web feature
named `NetInfoEffectiveType` (which was a pre-existing [Use Counter][Use
counters]). I.e.:

```cpp
IdentifiableSurface::FromTypeAndToken(
    Type::kWebFeature,
    WebFeature::NetInfoEffectiveType)
```

### Example: Media Capabilities {#eg-media-capabilities}

The [Media Capabilities API] helps determine whether some media type is
supported. E.g.:

```js
await navigator.mediaCapabilities.decodingInfo({
  type: "file",
  audio: { contentType: "audio/mp3" }
});
```

In this case the script is specifying a [`MediaDecodingConfiguration`]
dictionary. The [`MediaCapabilitiesInfo`] object returned by [`decodingInfo()`]
depends on the input. Hence we have to capture the input in the
`IdentifiableSurface` as follows:

```cpp
IdentifiableSurface::FromTypeAndToken(
    Type::kHTMLMediaElement_CanPlayType,
    IdentifiabilityBenignStringToken(mime_type))
```

See [Instrumentation] for more details on how to represent operation arguments
and caveats around encoding strings.

### Example: Media Streams API {#eg-media-streams}

Another more complicated example is this use of the [Media Streams API].

```js
var mediaStream = await navigator.mediaDevices.getUserMedia({video: {
  height: 240,
  width: 320
}});

var firstAudioTrack = mediaStream.getAudioTracks()[0];

var capabilities = firstAudioTrack.getCapabilities();
```

The target identifiable surface is the value of `capabilities`.

An important consideration here is that [`MediaDevices.getUserMedia`] operation
involves user interaction.

In theory, if the `getUserMedia` operation is successful, the
`IdentifiableSurface` for `capabilities` should represent the artifacts
(starting with the last global singleton object):

1. The operation [`MediaDevices.getUserMedia`].

1. A [`MediaStreamConstraints`] dictionary with value
   `{video: {height: 240, width: 320}}`.

1. The user action.

1. The operation [`MediaStream.getAudioTracks`] -- which is invoked on the
   result of the prior step assuming the operation succeeded.

1. `[0]`^th^ index -- applied to the list of [`MediaStreamTrack`]s resulting from
   the previous step

   > The Media Streams API does not specify the order of tracks. In general
   > where a spec doesn't state the ordering of a sequence, the ordering itself
   > can be a tracking concern. However in this case the implementation yields
   > at most one audio track after a successful `getUserMedia` invocation.
   > Hence there's no ordering concern here at least in Chromium.

1. The operation [`MediaStreamTrack.getCapabilities`] -- which is invoked on
   the result of the prior step.

However,

* The user action is not observable by the document. The only outcome exposed
  to the document is whether `getUserMedia()` returned a `MediaStream` or if
  the request was rejected due to some reason.

  It's not necessary to go beyond what the document can observe.

* If the call is successful the initial state of the resulting `MediaStream`
  determines the stable properties that a document can observe.

  The remaining accessors (e.g. `getAudioTracks()`, `getVideoTracks()` etc...)
  deterministically depend on the returned `MediaStream` with the exception of
  the indexing in step 5 which can be non-deterministic if there is more than
  one audio track.

  The diversity of document exposed state past step 3 is a subset of the
  diversity of the initial `MediaStream` object.

* If the call is rejected due to the request being over-constrained, then the
  exception could indicate limitations of the underlying devices.

Considering the above, we can tease apart multiple identifiable surfaces:

1. **VALID** The mapping from &lt;`"MediaDevices.getUserMedia"` operation,
   `MediaStreamConstraints` instance&gt; to &lt;`Exception` instance&gt; when
   the call rejects prior to any user interaction.

1. **OUT OF SCOPE** The mapping from &lt;`"MediaDevices.getUserMedia"`
   operation, `MediaStreamConstraints` instance&gt; to &lt;time elapsed&gt; when
   the call resolves.

   Timing vectors like this are outside the scope of the initial study.

1. **INFEASIBLE** The mapping from &lt;`"MediaDevices.getUserMedia"` operation,
   &lt;`MediaStreamConstraints` instance, (user action)&gt; to
   &lt;`MediaStream` instance&gt;.

   As mentioned earlier the user action is not exposed to the document. Hence
   we end up with an incomplete metric where the key doesn't have sufficient
   diversity to account for the outcomes.

1. **INFEASIBLE** The mapping from &lt;`"MediaDevices.getUserMedia"` operation,
   `MediaStreamConstraints` instance, (user action)&gt; to &lt;`Exception`
   instance&gt;.

   Same problem as above.

1. **VALID** The mapping from &lt;`MediaStreamTrack` instance&gt; to
   &lt;`MediaTrackCapabilities` instance&gt;.

   We can reason that this mapping is going to be surjective. The diversity of
   &lt;`MediaTrackCapabilities` instance&gt; is not going to add information.

   For simplicity surjective mappings can be collapsed into a single point
   without losing information. Thus the mapping here is just &lt;`MediaStream`
   instance&gt; to &lt;`1`&gt; where the value is arbitrary and doesn't matter.

1. **VALID** The mapping from &lt;`MediaStreamTrack.label` string&gt; to
   &lt;`MediaTrackCapabilities` instance&gt;.

   The label is a string like "Internal microphone" which can be presented to
   the user and assumed to be discerning enough that the user will find the
   string sufficient to identify the correct device.

The metrics we can derive from this surface are marked as **VALID**.

Constructing a digest out of any of the dictionary instances also requires some
care. Only include properties of each object that are expected to persist
across browsing contexts. For example, any identifier that is origin-local,
document-local, or depends on input from the document is not a good candidate.

<!-- sort, case insensitive -->
[`blink::IdentifiableSurface`]: ../../third_party/blink/public/common/privacy_budget/identifiable_surface.h
[`blink::WebFeature`]: ../../third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom
[`decodingInfo()`]: https://www.w3.org/TR/media-capabilities/#dom-mediacapabilities-decodinginfo
[`HTMLMediaElement.canPlayType()`]: https://developer.mozilla.org/en-US/docs/Web/API/HTMLMediaElement/canPlayType
[`MediaCapabilitiesInfo`]: https://www.w3.org/TR/media-capabilities/#dictdef-mediacapabilitiesinfo
[`MediaDecodingConfiguration`]: https://developer.mozilla.org/en-US/docs/Web/API/MediaDecodingConfiguration
[`MediaDevices.getUserMedia`]: https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia
[`MediaStream.getAudioTracks`]: https://developer.mozilla.org/en-US/docs/Web/API/MediaStream/getAudioTracks
[`MediaStream`]: https://developer.mozilla.org/en-US/docs/Web/API/MediaStream
[`MediaStreamConstraints`]: https://developer.mozilla.org/en-US/docs/Web/API/MediaTrackConstraints
[`MediaStreamTrack.getCapabilities`]: https://developer.mozilla.org/en-US/docs/Web/API/MediaStreamTrack/getCapabilities
[`MediaStreamTrack`]: https://developer.mozilla.org/en-US/docs/Web/API/MediaStreamTrack
[`NetworkInformation.saveData`]: https://developer.mozilla.org/en-US/docs/Web/API/NetworkInformation/saveData
[`window.screenTop`]: https://developer.mozilla.org/en-US/docs/Web/API/Window/screenTop
[Adding a Surface Type]: privacy_budget_instrumentation.md#adding-a-surface-type
[direct surface]: privacy_budget_glossary.md#directsurface
[Instrumentation]: privacy_budget_instrumentation.md
[Instrumenting Direct Surfaces]: privacy_budget_instrumentation.md#annotating-direct-surfaces
[Media Capabilities API]: https://developer.mozilla.org/en-US/docs/Web/API/Media_Capabilities_API
[Media Streams API]: https://developer.mozilla.org/en-US/docs/Web/API/Media_Streams_API
[this doesn't scale]: https://thecooperreview.com/10-tricks-appear-smart-meetings/
[Use Counters]: ../use_counter_wiki.md
