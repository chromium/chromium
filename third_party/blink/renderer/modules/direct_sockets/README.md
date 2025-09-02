This directory implements the Blink side of the [Direct Sockets
API](https://github.com/WICG/direct-sockets/blob/main/docs/explainer.md)
and [Multicast API](https://github.com/explainers-by-googlers/multicast-in-direct-sockets).

It will connect to a DirectSocketsService Mojo service in the browser,
which performs security checks, provides dialogs, and forwards requests to
the Network service.
