# Privacy Budget: Instrumentation

Refer to [Privacy Budget: Code Locations](privacy_budget_code_locations.md) for
details on where the code is located.

All instrumentation for the identifiability study is done via the API exposed in
[`third_party/blink/public/common/privacy_budget`](../../third_party/blink/public/common/privacy_budget).

Follow the instructions below for adding instrumentation for an API.

1. Are you annotating a [direct surface]? Jump to [Annotating Direct
   Surfaces](#annotating-direct-surfaces) below.

1. Are you annotating a [volatile surface]? For now we aren't annotating those.
   So let's go ahead and pick a different API to work on.

1. Determine the `UkmSourceId` and `UkmRecorder` to use for reporting, which
   depends on what you have. See the table below:

   | You have this              | Use this                                                                |
   |----------------------------|-------------------------------------------------------------------------|
   |[`blink::Document`]         |`Document::UkmRecorder()` and `Document::UkmSourceID()`                  |
   |[`blink::ExecutionContext`] |`ExecutionContext::UkmRecorder()` and `ExecutionContext::UkmSourceID()`  |

   Several classes inherit `blink::ExecutionContext` and therefore implement
   `UkmRecorder()` and `UkmSourceID()` methods. E.g.:

   * `blink::LocalDOMWindow`
   * `blink::WorkerGlobalScope`
     * `ServiceWorkerGlobalScope`
     * `SharedWorkerGlobalScope`
     * `DedicatedWorker`

   If you can get your hand on any of these, you are all set. Otherwise you may
   need to plumb a `UkmSourceID` down the stack.

   The only requirement as far as Privacy Budget is concerned is that the given
   source ID can be mapped to a top level navigation.

1. Decide on the [`blink::IdentifiableSurface`] to use, and the method for
   constructing it. If there's no corresponding surface type, see the
   [Surface Types](#surface-types) section for instructions on adding a new type.

   *** note
   What's a good candidate for [`blink::IdentifiableSurface`]?
   See [What's a good candidate for IdentifiableSurface?] below.
   ***

1. Condition all additional work on whether the study is active and whether the
   type and surface should be sampled. {#gating}

   On both the browser process and the renderer process the global
   [`blink::IdentifiabilityStudySettings`] singleton knows whether the study is
   active or not and whether a given [`blink::IdentifiableSurface`] or
   [`blink::IdentifiableSurface::Type`] should be sampled.

   If you know the final `blink::IdentifiableSurface` that you are going to use,
   then:

   ``` cpp
   #include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

   /* ... */

   if (IdentifiabilityStudySettings::Get()->ShouldSampleSurface(my_surface)) {
     // Only do work here.
   }
   ```

   Otherwise, if you know the surface type, then:

   ``` cpp
   #include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

   /* ... */

   if (IdentifiabilityStudySettings::Get()->ShouldSampleType(my_surface_type)) {
     // Only do work here.
   }
   ```

   If you neither know the surface nor the type (this happens if the code you
   are working on is common to a number of different surfaces or types) then:

   ``` cpp
   #include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

   /* ... */

   if (IdentifiabilityStudySettings::Get()->IsActive()) {
     // Only do work here.
   }
   ```

   *** note
   **Important**: There should be zero additional work done if the study is not
   active. If the surface or the type is disabled, then corresponding sampling
   code should do no work.

   This includes calculating the `IdentifiableSurface` and any related digests.
   If calculating the `IdentifiableSurface` is expensive, then the code should
   check `ShouldSampleType()` before progressing.

   The primary mechanism for recovering from an unforeseen adverse effect of
   sampling a surface is to stop the collection of that specific sample by way
   of the global [`blink::IdentifiabilityStudySettings`]. The success of this
   depends entirely on whether turning off the surface or type results in the
   sampling code not doing any work when the surface or type is disabled.

   If not, we'd need to shut down progressively larger scopes (i.e. the type or
   the entire experiment). Granular checks are important to minimize the fallout
   of such an event.
   ***

   *** promo
   Avoid using `IdentifiabilityStudySettings::Get()->IsActive()` except as a
   last resort. Use one of the more granular conditions instead.

   The decision on whether to activate the study is made on a per client (i.e.
   a browser instance) basis. It's not based on user profile nor any property
   associated with the user.
   ***

1. Calculate the sample value based on the documentation in
   [`identifiable_surface.h`] or whatever criteria you came up with.

   More specific guidelines and low level details about deriving
   an `IdentifiableToken` can be found in [`identifiable_token.h`]. However, the
   gist of it is:

   *** note
   **WAIT**: Is the value being derived from a string that could potentially
   contain sensitive information? (E.g. all strings originating from the
   document that is not defined as a constant somewhere in the browser is likely
   going to be sensitive). Then only extract a maximum of 16 bits from the
   string.

   For `blink::String` a.k.a. `WTF::String` types, this is implemented as
   `IdentifiabilitySensitiveStringToken()` and
   `IdentifiabilitySensitiveCaseFoldingStringToken()` in
   [`identifiability_digest_helpers.h`].
   ***

   * If you are about to construct a token out of a `WTF::String` type use
     one of the helpers in [`identifiability_digest_helpers.h`]. As mentioned
     above, you'll need to decide:

     * Whether this is a hardcoded string or not.
     * Whether the case of the string is relevant or not.

     It should be obvious which of the variants to use based on that. E.g.:

     ``` cpp
     namespace blink {

     void Foo(const String& input) {
       auto sample_value =
           IdentifiabilitySensitiveCaseFoldingStringToken(input);
       // ... other stuff
     }

     }  // namespace blink
     ```

   * For scalar types, the implicit conversions of
     [`IdentifiableToken`][`identifiable_token.h`] should be enough. I.e.:

     ``` cpp
     // `scalar_value` is an intrinsic integral or floating point type. So
     // implicit conversion kicks in.
     blink::IdentifiableToken sample_value = scalar_value;
     ```

     Or

     ``` cpp
     int scalar_input = GetInput();
     float scalar_value = GetValue();

     IdentifiabiltyMetricsBuilder(source_id)
       .Set(IdentifiableSurface::Type::kFoo,
            scalar_input,
            scalar_value)
       .Record(/* elided */);
     ```

1. Report the value.

## Surface Types {#surface-types}

Every identifiable surface value should be _aggregateable_ across clients.

*** promo
What does _aggregateable_ mean? Let's say we observe two samples for the same
[`blink::IdentifiableSurface`] 𝑣₁ and 𝑣₂. We should be able to claim that 𝑣₁
= 𝑣₂ implies that the underlying sources of entropy are in identical states.
Essentially this means that observations of a single identifiable surface value
are measuring the **same thing**.
***

For example: The [`Screen`]`.width` attribute corresponds to the identifiable
surface constructed as:

```cpp
// Good
auto surface = blink::IdentifiableSurface::FromTypeAndToken(
    Type::kWebFeature,
    WebFeature::kV8Screen_Width_AttributeGetter);
```

All values for this surface are measuring the pixel width of **the same
display**. All browser contexts active on the same browser window will report
the same value.

Another example: The [`Plugin`]`.filename` attribute may be considered to
correspond to the identifiable surface constructed as:

```cpp
// Bad
auto surface = blink::IdentifiableSurface::FromTypeAndToken(
    Type::kWebFeature,
    WebFeature::kPluginFilename);
```

However two observed values for this surface can't be meaningfully compared.
Their equivalence likely indicates that they are for the same plugin. However
if they are different, that could be because the two values correspond to
different plugins, or different versions of the same plugin. In the former
case, the same browser context can produce both values, which is misleading.
In the case of this specific attribute, the surface must be further keyed
based on some stable identifier for the plugin. For example, the key could be
derived from [`Plugin`]`.name`.

Whenever we are looking at two distinct sources of keys, the surfaces should
belong to two different types. I.e. two different
[`blink::IdentifiableSurface::Type`]`s`. If a matching type doesn't exist,
you'll need to add one. See the next section for how to do that.

### Adding a Surface Type {#adding-a-surface-type}

All surface types and their parameters must be documented in
[`identifiable_surface.h`]. When adding a new type, you should document:

1. The source of the key with enough detail for someone to independently
   construct the correct surface key.

2. How to compute the value with enough detail for someone to independently
   construct the correct value. Different code locations where the same surface
   type is sampled **must** produce the same `IdentifiableSurface` value given
   the same inputs.

For an example, see the comments above the definition of `kWebFeature` in
[`identifiable_surface.h`].

## Annotating Direct Surfaces via WebIDL Bindings {#annotating-direct-surfaces}

Since [direct surface]s are quite common, instrumentation for those APIs are done
via the Blink bindings generation process.

Operations and attributes (but not interfaces) that have been identified as
being useful for client fingerprinting receive the [`HighEntropy`] extended
attribute as follows:

``` idl
[Exposed=Window]
interface MyInterface : EventTarget {
    // This method does not expose any information.
    void uninterestingMethod();

    // This method, on the other hand, can be informative. It is marked as
    // HighEntropy. This annotation alone doesn't provide any automated
    // instrumentation since the bindings doesn't know what to look for or the
    // relevance of the input parameters.
    [HighEntropy, Measure] unsigned long? userHeightInInches(boolean tippyToes);

    // All the returned contents of this attribute including their ordering is
    // relevant for identification. It has the additional `Direct` token which
    // signals to the bindings generator to emit sampling instrumentation.
    [HighEntropy=Direct, Measure] readonly attribute DOMStringList allergies;
};
```

All [`HighEntropy`] attributes must be accompanied by a corresponding
[`Measure`] or `MeasureAs` attribute.

If there was no [`Measure`] or `MeasureAs` attribute then adding it also
involves updating `enums.xml` and `web_feature.mojom` as described in
[`Measure`]. Perhaps it's easier to follow an example like the one below.

Here's a sample CL that shows what needs to be done:
  * http://crrev.com/c/2351957: Adds IDL based instrumentation for
    `Screen.internal` and `Screen.primary`.

Don't add custom `UseCounter` enums and instead rely on the generated
`UseCounter` name whenever possible.

E.g.: This is preferred.

``` idl
[Exposed=Window]
interface MyInterface : EventTarget {
    [HighEntropy=Direct, Measure] readonly attribute unsigned short angle;
    [HighEntropy=Direct, Measure] readonly attribute DOMString type;
};
```

Over this...

``` idl
[Exposed=Window]
interface MyInterface : EventTarget {
    [HighEntropy=Direct, MeasureAs=MyCustomUseCounterName1] readonly attribute unsigned short angle;
    [HighEntropy=Direct, MeasureAs=MyCustomUseCounterName2] readonly attribute DOMString type;

    //                             ^^^^^^^^^^^^^^^^^^^^^^^
    //    You can optionally explicitly specify the UseCounter name like so.
    //    It's more meant for cases where two or more methods or attributes are
    //    effectively aliases of each other or the UseCounter only intends to
    //    measure usage of either.
};
```

There's no hard rule about this, but the `UseCounter` name is an implementation
detail that doesn't belong in the IDL. It also adds unnecessary noise.

*** note
**IMPORTANT** Make sure that each API has its own `UseCounter` name. Otherwise
multiple APIs will have their samples aggregated within the same bucket. This
alters the observed characteristics of the API from what it really is.
***

<!-- Sort (case insensitive), but don't line-wrap -->
[`blink::Document`]: ../../third_party/blink/renderer/core/dom/document.h
[`blink::ExecutionContext`]: ../../third_party/blink/renderer/core/execution_context/execution_context.h
[`blink::IdentifiabilityStudySettings`]: ../../third_party/blink/public/common/privacy_budget/identifiability_study_settings.h
[`blink::IdentifiableSurface::Type`]: ../../third_party/blink/public/common/privacy_budget/identifiable_surface.h
[`blink::IdentifiableSurface`]: ../../third_party/blink/public/common/privacy_budget/identifiable_surface.h
[`blink::WebFeature`]: ../../third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom
[`HighEntropy`]: ../../third_party/blink/renderer/bindings/IDLExtendedAttributes.md#HighEntropy_m_a_c
[`identifiability_digest_helpers.h`]: ../../third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h
[`identifiable_surface.h`]: ../../third_party/blink/public/common/privacy_budget/identifiable_surface.h
[`identifiable_token.h`]: ../../third_party/blink/public/common/privacy_budget/identifiable_token.h
[`Measure`]: ../../third_party/blink/renderer/bindings/IDLExtendedAttributes.md#Measure_i_m_a_c
[`Plugin`]: ../../third_party/blink/renderer/modules/plugins/plugin.idl
[`Screen`]: ../../third_party/blink/renderer/core/frame/screen.idl
[direct surface]: privacy_budget_glossary.md#directsurface
[Use Counter]: ../use_counter_wiki.md
[volatile surface]: privacy_budget_glossary.md#volatilesurface
[What's a good candidate for IdentifiableSurface?]: good_identifiable_surface.md
