# Network Service.

This is a service for networking. It's meant to be oblivious to Chrome's features.
Some design goals
  * this only contains features that go over the network. e.g. no file loading, data URLs etc...
  * only the lowest-level of networking should be here. e.g. http, sockets, web sockets. Anything that is built on top of this should be in higher layers.
  * higher level web platform and browser features should be built outside of this code. Safe browsing, Service Worker, extensions, devtools etc... should not have hooks here. The only exception is when it's impossible for these features to function without some hooks in the network service. In that case, we add the minimal code required. Some examples included traffic shaping for devtools and CORB blocking.
  * every PostTask, thread hop and process hop (IPC) should be counted carefully as they introduce delays which could harm this performance critical code.
  * `NetworkContext` and `NetworkService` are trusted interfaces that aren't meant to be sent to the renderer. Only the browser should have access to them.

See https://bugs.chromium.org/p/chromium/issues/detail?id=598073

See the design doc
https://docs.google.com/document/d/1wAHLw9h7gGuqJNCgG1mP1BmLtCGfZ2pys-PdZQ1vg7M/edit?pref=2&pli=1#
