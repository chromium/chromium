# The architecture of Browser/ChromeOS code

## Overview

We want to have clean code, good architecture, and clear ownership to ensure
everybody can efficiently deliver high quality products. Toward this goal, this
document discusses how code in the Chromium git repository is and should be
architected between the browser and ChromeOS code.

## Background

Originally, ChromeOS was just the Linux Chrome browser with a few extra
additions for UI system management. As such, and to keep the system requirements
very low, the entire ChromeOS UI was built into the Chrome "browser" process.
Over time, ChromeOS has gotten substantially more sophisticated and capable.
Many important services run in separate processes, services, or VMs, but most of
the UI still runs in the main browser process.

The Lacros project aimed to separate the Linux processes and the software
releases between the browser and the OS shell by shipping the Chrome web browser
as a standalone app which could be updated independently of ChromeOS. Lacros
communicated between the Chrome browser and ChromeOS via an IPC interface called
crosapi. However, the Lacros project has been deprioritized and relevant code is
in the process of being deprecated and deleted.

### Definitions

- **Browser:** General term referring to a process with web browsing capabilities.

- **Ash:** The ChromeOS system UI. In this document, this term is used broadly
  to include most of the non-browser UI features including the app launcher, the
  system tray and notifications, the window manager, the system compositor, and
  the login UI.

- **Lacros (deprecate, being deleted):** The ChromeOS-specific browser that does
  not include Ash. This is similar to the Linux browser but with ChromeOS-
  specific features and integrations.

- **Ash Browser:** The "classic" (non-Lacros) ChromeOS software that includes
  Ash and the browser in one process.

- **Browser code:** Code required to build a browser. This includes
  platform-specific integrations with the host OS rather than just the
  cross-platform parts. For ChromeOS, this includes many important ChromeOS
  browser features but does not include anything considered "Ash."

- **OS code:** Any ChromeOS-specific code that isn’t "browser code." This is
  mostly Ash when referring to code in the Chromium repository.

- **Shared code:** Code used in both browser and OS code including //base,
  //mojo, //ui, and some components.

## Desired state

_This section describes the long-term architectural goal rather than the current
state or the current requirements. See below for what to do for current work._

The desired end-state is that "browser code" (including ChromeOS-specific
browser features) and "OS code" have a clear separation. Communication between
these layers should be done using well-defined APIs. Function calls in the code
happen "down" the stack from the browser to the OS, and any calls "up" from the
OS to the browser happen via events, observers, and callbacks configured by the
browser layers.

Shared code like //views may have ChromeOS-specific parts and take contributions
from anyone, but the Browser and OS teams should agree that the code is
appropriate for such sharing.

In this desired state:

- The //chrome directory is for the implementation of the Chrome browser. It
  should not have any OS code in it (for example, //chrome/browser/ash is
  undesirable) and OS code should not call directly into //chrome code outside
  of the above-mentioned callbacks.

- The //content directory is the API for building a web browser. Even though Ash
  does use web technology for rendering many things, it is not itself a web
  browser and there should be no OS code in this directory or calling directly
  into it.

- Browser code should only call into OS code through well-defined APIs (e.g.,
  extension APIs). This provides a conceptual separation between browser and OS
  concerns.

Not all parts of the product fit neatly into the browser and OS layers, with
extensions and apps being big examples. How web page embedding should be done
from Ash is an area of active design and there is not currently good guidance
for this. In these less well-defined areas, work toward as clear a separation as
practical given the current state and the long-term requirements of that
component.

## Current policies

New features should be designed to adhere to the "desire" state" as closely as
practical. However, it is currently not possible to implement all functionality
in Ash according to that state:

- Some functionality (e.g., the `Profile` class) is only available in //chrome,
  and there is no clear alternative to use.

- Legacy code still has significant //chrome dependencies and has not been
  migrated away from this state.

Thus, we must be pragmatic about implementing Ash features in the meantime,
using the following guidance:

- Any new Ash functionality should add its core functionality outside of
  //chrome.
  - In this context, "core" functionality includes the primary business logic of
    a feature.
  - Guidance on where this code should exist:
    - **Ash-only code which is not system UI:** //chromeos/ash/components
    - **Ash-only system UI code:** //ash
    - **Shared by both Ash and Lacros:**
      - *UI code:* //chromeos/ui
      - *Non-UI code:* //chromeos/components
      - **NOTE:** Lacros is in the process of being deprecated. Do not add any
        new Lacros code.
    - **Shared between ChromeOS (i.e., ash-chrome) and other platforms:**
      //components

- For code which must depend on //chrome, push logic down lower in the
  dependency graph as much as possible, and only implement a thin wrapper in
  //chrome. With this pattern, the code in //chrome is mostly "glue" or
  initialization code, which will minimize the effort required in the future to
  break these dependencies completely.
  - Example 1: Phone Hub's [`BrowserTabsModelProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/phonehub/browser_tabs_model_provider.h;drc=2a153c1bc9f24cae375eee3cc875903866997918)
    is declared in //chromeos/ash/components alongside related code logic, but
    [`BrowserTabsModelProviderImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/phonehub/browser_tabs_model_provider_impl.h;drc=fe132eeb21687c455d695d6af346f15454828d01)
    (in //chrome) implements the interface using a //chrome dependency.
  - Example 2: Phone Hub's [`PhoneHubManagerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/phonehub/phone_hub_manager_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf)
    is declared in //chromeos/ash/components and has dependencies outside of
    //chrome, but the concrete implementations of some of these components are
    [`KeyedService`](https://source.chromium.org/chromium/chromium/src/+/main:components/keyed_service/core/keyed_service.h;drc=d23075f3066f6aab6fd5f8446ea5dde3ebff1097)s
    requiring //chrome. In this case, [`PhoneHubManagerFactory`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/phonehub/phone_hub_manager_factory.h;drc=d23075f3066f6aab6fd5f8446ea5dde3ebff1097)
    instantiates [`PhoneHubManagerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/phonehub/phone_hub_manager_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf)
    in //chrome (serving as a thin wrapper around the dependencies), but the
    vast majority of logic is lower in the dependency graph.

- A few common //chrome dependencies that may be able to be broken easily:
  - Instead of using [`ProfileKeyedServiceFactory`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/profiles/profile_keyed_service_factory.h;drc=77a7a02b1822640e35cac72c0ddd7af7275eeb9b)
    (in //chrome), consider using [`BrowserContextKeyedServiceFactory`](https://source.chromium.org/chromium/chromium/src/+/main:components/keyed_service/content/browser_context_keyed_service_factory.h;drc=371515598109bf869e1acbe5ea67813fc1a4cc3d)
    (in //components) instead.
  - Instead of using a [`Profile`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/profiles/profile.h;l=308-311;drc=3f4203f7dca2f7e804f30cfa783e24f90acd9059)
    (in //chrome) to access user prefs, consider using
    [`User::GetProfilePrefs()`](https://source.chromium.org/chromium/chromium/src/+/main:components/user_manager/user.h;l=127-131;drc=e49b1aec9585b0a527c24502dd4b0ee94b142c3c)
    (in //components) instead.

- For any new code added in //chrome/browser/ash, a DEPS file must be created
  which explicitly declares //chrome dependencies. People residing in
  //chrome/OWNERS can help suggest alternatives to these dependencies if
  possible when reviewing the code which adds this new DEPS file. See
  [b/332805865](http://b/332805865) for more details.

If you need advice to help you make a decision regarding your design, please
reach out to ash-chrome-refactor@google.com for feedback.

## Path forward

The current policy aims to stop accumulating more undesirable OS/browser
dependencies while acknowledging there is a large amount of legacy code that
does not follow the guidelines. The team is moving toward the desired state
using the following approach outlined in go/ash-chrome-refactor:

- Ensure that all Ash code in //chrome is defined using granular BUILD.gn files.
  Each directory should define its own build targets and list dependencies
  explicitly.
  - See https://crbug.com/335314438, https://crbug.com/351889236.
- ...more to come as the Ash //chrome Refactor makes progress.
