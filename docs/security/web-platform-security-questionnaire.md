# Web Platform Security Questionnaire

The goal of this questionnaire is to help you understand if your web API might have a security impact. If you answer yes to any of the following question, your feature needs a security review. Note that even if you answer no to all of those questions, you can still benefit from a security review.

* Does your feature allow data to cross origins, including sending data to a cross-origin service (even owned by Google)?
  * *Example: you are surfacing performance timings for cross-origin subresources or you are reading properties from a cross-origin frame.*

* Does your feature introduce new timers or timing measurements?
  * *Example: you are adding a new field in the Performance Object.*

* Does your feature surface information about hardware (capabilities, usage)?
  * *Example: you are surfacing CPU utilization or memory usage.*

* Does your feature interact with how documents or resources are loaded?
  * *Example: you are blocking document load until a particular event happens, you are triggering Fetch requests in a new context.*

* Does your feature introduce a new way to execute JavaScript code?
  * *Example: you are introducing a new kind of execution context, or you are now executing JS at a point where it would normally not execute.*

* Does your feature introduce a new type of HTML element or a new MIME type?

* Is your feature gated behind a Permission?

* Does your feature depend on global state (i.e. unpartitioned state shared between different origins and sites)?
  * *Example: your feature has a maximum concurrent usage that is shared across all pages in a Profile.*

* Does your feature interact with cookies or other persistent storage on the client?

* Does your feature interact with any of the security policies of the web (CSP, CORS, COOP, COEP)?

*To help you develop safe web platform APIs, we also have [web platform security guidelines](https://chromium.googlesource.com/chromium/src/+/master/docs/security/web-platform-security-guidelines.md) and specific [stop leaks policies](https://chromium.googlesource.com/chromium/src/+/master/docs/security/stop-leaks-policy.md) that you can check.*
