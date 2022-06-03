# Privacy Budget: Glossary

<!-- Keep definitions sorted topologically, but also:

* Add a named anchor. See
  https://gerrit.googlesource.com/gitiles/+/HEAD/Documentation/markdown.md#Named-anchors

* Use [this kind of reference], which you can define at the bottom of this
  document.

-->

## Identifiable Surface {#identifiablesurface}

Concretely, defined in [`identifiable_surface.h`]. Represents a single source of
partially identifying information.

### Direct Identifiable Surface {#direct} {#directsurface}

An [identifiable surface](#identifiablesurface) where the value returned by an
operation or an attribute defined in an IDL file independently reports the
partial fingerprint. No additional contextual information is necessary to
interpret the meaning of the returned value.

E.g.: `Screen.width` which is exposed as `window.screen.width`.

Direct identifiable surfaces are a special case of [Keyed Identifiable
Surface](#keyedsurface)s where the key is implicit and global.

### Keyed Identifiable Surface {#keyedsurface}

An [identifiable surface](#identifiablesurface) where the partial fingerprint is
only meaningful if its context is fully qualified.

E.g.: `HTMLElement.scrollWidth` depends on the contents and styling of the
element. Hence for the return value to qualify as a partial fingerprint it needs
to be interpreted in the context of the content and styling.

### Identifiable Surface Type {#surfacetype}

See [Surface Types].

### Volatile Identifiable Surface {#volatilesurface}

An [identifiable surface](#identifiablesurface) which changes semi-frequently
but whose change events can be correlated to join identities across browsing
contexts.

<!-- References go here. Keep them sorted. -->
[`identifiable_surface.h`]: ../../third_party/blink/public/common/privacy_budget/identifiable_surface.h
[Surface Types]: privacy_budget_instrumentation.md#surface-types
