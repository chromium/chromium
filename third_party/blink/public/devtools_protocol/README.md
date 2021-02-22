# Chrome DevTools Protocol Contribution Guidelines

This document summarizes the design guidelines for the APIs exposed through
Chrome DevTools protocol (_CDP_ further in the document) and provides a
brief overview of CDP terminology and related DevTools backend architecture.

## API stability

Although the CDP was originally conceived with Chrome DevTools front-end as
the primary client, it is currently used by multiple clients, most of which
reside outside of Chromium source tree. We aim at maintaining a reasonably
stable and future-proof API for such clients, so we offer certain
compatibility terms for the CDP API:

* The commands, events and types not marked as `experimental` are guaranteed
  to remain backwards compatible until the next version of the protocol
  after the one where they have been marked as deprecated.
  This implies that no new mandatory input parameters (or fields in input
  types) will be added, and no output parameters (or fields in output
  types) will be removed or will become optional.
* The commands, events and types marked as `deprecated` will remain supported
  until the protocol version is incremented. We will keep deprecated commands,
  events and types for at least 3 Chrome releases before they are removed.
* The commands, events and types marked as experimental may be changed at
  any time without a notice.

The following principles should help contributors to maintain a
comprehensive and stable API:

* The API should, to the extent practical, avoid exposing any chrome-specific
  implementation detail and should, when possible, be expressed in terms
  generally meaningful for the web platform, so that the same interface could
  be supported by a different browser implementation.
* Interfaces should be expressed in terms of strong types and, when possible,
  rely on built-in protocol type validation rather than explicitly implementing
  parameters validity and consistency checks.
* The functionality exposed through CDP should be limited to that immediately
  required by at least one client.
* The compatibility risks introduced by exposing of additional API surface
  should by justified by sound user stories for protocol clients.

## Domains, Commands and Events

- *Domains* are modules used to logically group related types, events and
commands, e.g. `Network`, `Performance` or `DOM`.

- *Commands* (occasionally referred to as "methods") are requests sent by the
client to the backend. Each command eventually produces a response indicating
completion, either successful or not. In case of success, the command may return
arbitrary number of output parameters. If the command has failed, it should
preferably indicate an error using protocol standard means (i.e. using static
methods of the `ProtocolResponse` class). However, in rare cases where command
requires additional error information, it should indicate success via the
protocol while returning additional error details through output parameters.
No explicit indication of success through method's output
parameters (e.g. `boolean success`) should be used.

- *Events* are notifications sent by the backend to the client and may carry
arbitrary parameters. Events do not require any acknowledgement by the client.
No events should be emitted by the backend before it was explicitly enabled
by the client (typically, through domain's "enable" command), or after
it was disabled.

- If a command invocation results in sending some events (for example, the
`enable` command of certain domains may result in sending of previously
buffered events), these are guaranteed to be emitted before the method
returns.

## Naming Convention

Types are named using PascalCase (AKA UpperCamelCase), e.g. `ResourceTiming`.
Methods, events, parameters and object properties are named using camelCase.

Methods should follow \<verb\>\[Object\] pattern. e.g. `enable`, `getCookies`,
`captureScreenshot` or `addScriptToEvaluateOnLoad`.

Event names should follow \<object\>\<Verb-in-passive-voice\> pattern, e.g.
`consoleMessageAdded` or `requestWillBeSent`.

## Agents, Targets and Sessions

*Agents* are backend classes that implement individual protocol domains. Some
agents are implemented in the renderer process (either in Blink or v8; no
agents are currently present in the content/renderer so far) and some are in
the browser process (implemented either by the content layer or by the embedder).
A domain may be implemented by multiple agents spanning several layers and
processes, e.g. have instances in `chrome/`, `content/` and `blink/renderer/ `.
A single command may be handled by multiple layers in the following sequence:
embedder, content browser, renderer.
An agent may indicate that it has completed handling the command or let the
command fall-through to the next layer.

For historical reasons, agents that are implemented in the browser process
are also called *handlers* and the classes that implement them are named
as *\<Domain*>Handler.

*Targets* are entities being inspected or debugged, such as frame subtrees,
workers or worklets of different types, or an external VMs (e.g. a nodejs
instance). Each target is identified by a UUID and has associated type
(e.g. `iframe`, `shared_worker` etc). The `browser` target handles methods
that have global effects on the entire browser (or on a certain profile).

The type of the target defines the set of protocol domains a given target
supports. For example, targets that support JavaScript execution (that is,
all except `browser`) would have `Runtime` domain, but only iframes would
have `DOM` domain.

While each worker or worklet corresponds to a target of its own, multiple
local frames belonging to the same page will be grouped to a
single target. A subframe thus may change its target during the navigation,
typically in case when it navigates into or out of the process of its parent.

A *session* corresponds to a single client connection to a particular target.
Agents appropriate for target types are instantiated once per session and
per layer -- for example, when a client connects to a frame, a PageHandler
from chrome/, a PageHandler from content/ and an InspectorPageAgent (from
blink) are instantiated for the given session.

## Multiple Sessions

DevTools support multiple sessions with the same target, which implies that
multiple agents for the same domain should be designed to co-exist.
For example, an agent should avoid overriding browser state modified by
another instance of the agent unless the protocol client explicitly
requested this.

## Cross-process navigation and state transfer

Among the implementation details hidden from the protocol client is the fact
that some frame navigations require render process swaps. This requires the
renderer-side agent to represent some of their state in a way that may be
replicated to a different renderer in the event of navigation.
State that was configured by the client and is not associated to current
document should be maintained using type aliases offered by
`InspectorAgentState` class.

## Security Considerations

Protocol clients are typcally considered trusted, as they can navigate to
arbitrary origins and have access to all origin data. However, since the
protocol is also exposed to chrome extensions through `chrome.debugger` API,
the backend implements additional access control in some of the methods to
prevent extensios form accessing file system or otherwise escaping the sandbox.
These restrictions are not extended to other types of clients.

Protocol clients should be prepared to handle data coming from untrusted
sources such as malicious web pages and potentially compromised renderer
processes.

## Object Identifiers

String identifiers are preferred to integers even when the underlying
implementation currently offers an integer identifier. This is so that
we have flexibility of using composite identifiers in the future to avoid
identifier collisions, for example, by prepending process identifier
to renderer-issued ids.

## Wire Format, Strings and Binary Values

CDP is designed with JSON-RPC 2.0 as the primary wire format (though other
representations exist). When exposed outside of the browser, the JSON
produced by the CDP bindings is encoded as UTF-8 in accordance with
RFC-8259. Some of the strings may come from JavaScript or DOM, where
strings are typically represented as UTF-16. This may result in strings
containing unpaired UTF-16 surrogates that doesn't have a matching UTF-8
representation. Such surrogates, as well as control characters that can't
appear as is in JSON would be escaped in accordance with RFC-8259.

Binary data, such as images or the contents of arbitrary network
requests, are designated with Binary type in the protocol definition,
which is mapped to base64-encoded strings when sent over JSON, and uses
a more efficient representation when protocol is represented using
a binary wire format.

## Localizability

Data passed over the protocol should be suitable for handling by automated
tools as well as UI clients supporting i18n, so passing messages (such as
errors) in English is rarely appropriate, structured types and enums
should be used instead.
