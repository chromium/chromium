# Writing a New Extension API

[TOC]

## Overview
This document describes the procedure and general advice for writing the
implementation of a new Extension API.

## Before Writing (Much) Code
Before you invest significant time and energy into writing a new Extension API,
be sure to go through the API proposal process.  This is important, as we may
not accept each proposal for a new Extension API, and many require tweaks or
changes.  The proposal process is designed to reach consensus on both the
general usefulness and appropriateness of an API, as well as the high-level
shape it will take.

The proposal process is documented [here](/extensions/docs/new_api_proposal.md).

## Implementation Concepts
Extension APIs are defined by their schema files.  Access to these APIs is
controlled entries in the features files.  APIs are generally implemented
through a combination of extension functions and events (and, occasionally,
properties).  APIs are exposed in JavaScript to the extension through extension
bindings.

### Schemas
An extension schema defines the API, including the functions, events, and
properties on the API.

Read more about schemas [here](/chrome/common/extensions/api/schemas.md).

### Features
Extension feature files control access to different APIs, and can restrict APIs
(or specific methods) to different types of extensions, Chromium release
channels, or even to specific extension IDs, as well as specify required
permissions.

Read more about the features files
[here](/chrome/common/extensions/api/_features.md).

### API Functions
Extension functions are called by the extension in order to perform some action
in the Chromium browser.  For instance, the `chrome.tabs.create()` API function
is called by an extension to create a tab, and is implemented in C++ by the
`TabsCreateFunction`, and instance of the `ExtensionFunction` class.  Generally,
each API function will map to an instance of the `ExtensionFunction` class.

Read more about extension functions [here](/extensions/docs/api_functions.md).

### API Events
Events are dispatched by Chrome to inform the extension of an occurrence.  For
instance, the `chrome.tabs.onCreated()` event is dispatched when a new tab is
created.

Read more about extension events [here](/extensions/docs/events.md).

### API Properties
Properties on the API are exposed as JavaScript properties on the API object
itself.  These are generally rare.  Constants defined in the API (such as
`chrome.tabs.TAB_ID_NONE`) are exposed automatically through the bindings layer.
More complex objects (such as the `StorageArea` defined in `chrome.storage` for
`chrome.storage.local`, `chrome.storage.sync`, and `chrome.storage.managed`)
need to be defined in the API, and are constructed by the bindings layer.

### Extension Bindings
The bindings system is responsible for creating the JavaScript entry points
that extensions use to invoke extension APIs, according to the definitions in
the schema and the features files.  **Most APIs should not need any special code
in extension API bindings, and custom bindings are generally discouraged.**
Custom bindings are only required if an API has behavior that is unique enough
to not be built into the general extension API system.

Read more about extension bindings [here](/extensions/renderer/bindings.md).

## Implementation Process
What is the best way to approach writing a new API implementation?

### Development
#### Include a new OWNERS file
The proposal process requires a team to sign on for continued ownership and
stewardship of an API.  Any extension API that is not being designed and
implemented by the core extensions team (and some that are) should have a
separate OWNERS file.

#### Include //extensions OWNERS on CLs
Even though each API should have its own dedicated OWNERS, it's good practice
to include an OWNER from [//extensions/OWNERS](/extensions/OWNERS) to review the
interaction with the core extensions system.  We can offer guidance on the use
of the different core concepts of API implementation, and ensure that the new
code is following best practices.

#### Start enabled on "trunk" or "canary"
During the development process, the extension API should start restricted to
"trunk" or "canary" in the features files (ideally "trunk", which means it is
only accessible when building from source; "canary" should only be used if it
is necessary to have external testers for the verification of the API).  Things
tend to change during development, and we don't want an API to reach stable
channel before it's ready or when it is likely to experience churn.

### Writing Code
Now, it's time to actually write the code to implement the API!

#### Approaches
As a general practice in Chromium, it's good to develop CLs that represent a
full logical unit, complete with tests.  This does not have to mean it has to
be entirely complete - it may not even be reachable in production code.
However, it should be clear to reviewers what the functionality is, and that it
is tested and works as intended.

This applies to writing new APIs, as well. APIs should frequently be written
piecemeal, in multiple CLs, with each CL having a logical unit of tested code.
For example, a CL may include:
* An entry in the API schema (for instance, a new function or event)

* The implementation of the new capability (the implementation of that function
  or dispatching that event appropriately)

* Tests for the capability (a unit test, API test, or both)

In this case, the logical unit is the new function or event, complete with tests.

For exceptionally large or complex APIs, even this may be too large of a first
step, and smaller CLs may be required before even creating the API entry (for
instance, a new API function may require changes elsewhere in Chromium to
enable a new behavior).

This approach makes it easy for reviewers to review the CL in a reasonable
period of time, and keeps development of the API moving.

#### Anti-Approaches
The below are discouraged.

**The All-in-One CL:**
In most cases, please do not try and fit an entire API implementation into a
single CL.  This typically results in a large, unwieldy CL that is difficult to
review.  Review times generally increase superlinearly (i.e., faster than
linearly) with the number of lines added - a 200 line CL is usually more than
5x faster to review than a 1,000 line CL.

**The Stubbed-Out CL:**
A common anti-approach is to add an API stub as the first CL, where that stub
adds the entirety of the API surface and empty extension function
implementations for each API function.

First, this makes it impossible to evaluate the correctness of the API
implementation and usage of extension system concepts.  Adding a new API
method, intentionally, requires special review from API reviewers (who are
familiar with the best practices and any common pitfalls).  If a stub is used,
this review is no longer useful.

Additionally, APIs may need to be slightly tweaked as a result of different
implementation details.  While most of these should be ironed out in the
proposal process, some may still come up during the implementation review.
Bundling the declaration with the implementation allows us to catch any changes
that need to happen in the API surface during the primary review.

#### Code Concepts
TODO(devlin): Incorporate the below into this article.

This [article](https://www.chromium.org/developers/design-documents/extensions/proposed-changes/creating-new-apis)
describes a number of different code concepts, and can be useful for some of
the high-level approaches.  Note that some of this is outdated.

### Launching

#### Add and Verify Documentation
Much of the documentation for extension APIs is auto-generated from the schema
files.  This includes method signatures and descriptions and type descriptions.
If you don't require any additional documentation, the only required step is to
add a new template article in
`chrome/common/extensions/docs/templates/public/extensions`.  If you need
additional documentation, you can also add an article in
`chrome/common/extensions/docs/templates/intros`.

To verify the documentation is correctly included and visible, run the preview
mode of the documentation server by running
`chrome/common/extensions/docs/server2/preview.py` and visiting
`localhost:8000/extensions/<apiName>`.

#### Adjust Features Files
Once an API has been fully implemented, tested, and is, in fact, stable, the
features file restriction can be lifted.  Depending on the complexity of the
API, it may also need periods of restriction in "dev" and/or "beta".
