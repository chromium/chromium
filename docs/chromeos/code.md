# The architecture of Browser/ChromeOS code

## Overview

We want to have clean code, good architecture, and clear ownership to ensure
everybody can efficiently deliver high quality products. Toward this goal, this
document discusses how code in the Chromium git repository is and should be
architected between the browser and ChromeOS code.

## Background

Originally, ChromeOS was just the Linux Chrome browser with a few extra
additions for UI system management. As such, and to keep the system requirements
very low, the entire ChromeOS UI was built into the Chrome “browser” process.
Over time, ChromeOS has gotten substantially more sophisticated and capable.
Many important services run in separate processes, services, or VMs, but most of
the UI still runs in the main browser process.

The Lacros project aims to separate the Linux processes and the software
releases between the browser and the OS shell. But Lacros does not by itself
move any code: Lacros only converts what would otherwise be abstract C++
interfaces and internal APIs to IPC calls. This document deals with the code
layout and abstractions which is independent from Lacros.

### Definitions

- **Browser:** General term referring to a process with web browsing capabilities.

- **Ash:** The ChromeOS system UI. In this document, this term is used broadly
  to include most of the non-browser UI features including the app launcher, the
  system tray and notifications, the window manager, the system compositor, and
  the login UI.

- **Lacros:** The ChromeOS-specific browser that does not include Ash. This is
  similar to the Linux browser but with ChromeOS-specific features and
  integrations.

- **Ash Browser:** The “classic” (non-Lacros) ChromeOS software that includes
  Ash and the browser in one process.

- **Browser code:** Code required to build a browser. This includes
  platform-specific integrations with the host OS rather than just the
  cross-platform parts. For ChromeOS, this includes many important ChromeOS
  browser features but does not include anything considered “Ash.”

- **OS code:** Any ChromeOS-specific code that isn’t “browser code.” This is
  mostly Ash when referring to code in the Chromium repository.

- **Shared code:** Code used in both browser and OS code including //base,
  //mojo, //ui, and some components.

## Desired state

_This section describes the long-term architectural goal rather than the current
state or the current requirements. See below for what to do for current work._

The desired end-state is that “browser code” (including ChromeOS-specific
browser features) and “OS code” have a clear separation. Communication between
these layers should be done using well-defined APIs. Function calls in the code
happen “down” the stack from the browser to the OS, and any calls “up” from the
OS to the browser happen via events, observers, and callbacks configured by the
browser layers.

Shared code like //views may have ChromeOS-specific parts and take contributions
from anyone, but the Browser and OS teams should agree that the code is
appropriate for such sharing.

In this desired state:

- The //chrome directory is for the implementation of the Chrome browser,
  including Lacros. It should not have any OS code in it (for example,
  //chrome/browser/ash is undesirable) and OS code should not call directly into
  //chrome code outside of the above-mentioned callbacks.

- The //content directory is the API for building a web browser. Even though Ash
  does use web technology for rendering many things, it is not itself a web
  browser and there should be no OS code in this directory or calling directly
  into it.

- Browser code should only call into OS code through well-defined APIs
  (“crosapi”). In addition to addressing the practical cross-process
  requirements of Lacros, this provides a conceptual separation between browser
  and OS concerns.

Not all parts of the product fit neatly into the browser and OS layers, with
extensions and apps being big examples. How web page embedding should be done
from Ash is an area of active design and there is not currently good guidance
for this. In these less well-defined areas, work toward as clear a separation as
practical given the current state and the long-term requirements of that
component.

## Current policies

New features should be designed to adhere to the “desired state” as closely as
practical. Due to the volume of legacy code it is often impossible to achieve
100% compliance with the desired state so we have to be pragmatic. In
particular:

- New ChromeOS features should avoid compile-time dependencies on //chrome.
  - Exceptions are permitted, especially when interacting with legacy code that
    does not follow these rules.
  - When you can’t implement the “right” design, get as close as possible and
    try to make it as easy as possible to migrate in the future (see
    “Implementation advice” below).

- Existing OS-to-browser dependencies will be grandfathered in via DEPS.
  - Modification of existing code is permitted, including modifications that
    require adding new DEPS entries when necessary. The DEPS file is there today
    to help flag issues that may need more thought rather than prevent
    additions.
  - Try to move incrementally toward the desired goal.

- Undesirable browser-to-OS dependencies are enforced by the linking of the
  Lacros browser without Ash code.

Reach out to crosapi-council@ for advice.

## The path forward

The current policy aims to stop accumulating more undesirable OS/browser
dependencies while acknowledging there is a large amount of legacy code that
does not follow the guidelines.

The Lacros project has been creating a clear OS API (“crosapi”) to provide the
correct browser-to-OS calls.

For the OS-to-browser calls, there is no current project staffed to clean up all
of the existing code dependencies, the biggest example being the existence of
//chrome/browser/ash. As such, there is no schedule or cost/benefit analysis for
such work, but efforts to improve the situation are welcome.

The plan is to increase the requirements over time to move in the direction of
the architectural goal. This will likely take the form of additional levels of
review for new includes of browser headers from OS code.
