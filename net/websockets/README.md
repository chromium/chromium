# WebSocket protocol

This directory contains the implementation of
[the WebSocket protocol](https://tools.ietf.org/html/rfc6455).

## Design docs

* [WebSocketBasicHandshakeStream design
  memo](https://docs.google.com/document/d/1r7dQDA9AQBD_kOk-z-yMi0WgLQZ-5m7psMO5pYLFUL8/edit).
  Some details have changed, but still a mostly-accurate description of
  Chromium's current implementation.
* [WebSocket Throttling
  Design](https://docs.google.com/document/d/1a8sUFQsbN5uve7ziW61ATkrFr3o9A-Tiyw8ig6T3puA/edit)
  discusses how we enforce WebSocket connection throttling. Also contains
  detailed discussion of how WebSockets integrate with the socket pools. Dates
  from 2014, but still mostly relevant.
* [WebSockets over
  HTTP/2](https://docs.google.com/document/d/1ZxaHz4j2BDMa1aI5CQHMjtFI3UxGT459pjYv4To9rFY/edit).
  Current as of 2019 description of WebSocket over H/2 implementation.
* [WebSocket + Network Service + WebRequest
  API](https://docs.google.com/document/d/1L85aXX-m5NaV-g223lH7kKB2HPg6kMi1cjrDVeEptE8/edit):
  design for how extension callbacks are called when the network service is
  enabled.
* [WebSocket HTTP Auth
  Design](https://docs.google.com/document/d/129rLtf5x3hvhP5rayLiSxnEjOXS8Z7EnLJgBL4CdwjI/edit).
  This document is very low on detail, but can serve as an overview of how auth
  works for WebSockets.
* [Per-renderer WebSocket
  throttling](https://docs.google.com/document/d/1aw2oN5PKfk-1gLnBrlv1OwLA8K3-ykM2ckwX2lubTg4/edit).
  While the algorithm described in this document is still used, the code has
  moved around significantly due to network servicification.
* [WebSocket Protocol Stack in
  chrome/net](https://docs.google.com/document/d/11n3hpwb9lD9YVqnjX3OwzE_jHgTmKIqd6GvXE9bDGUg/edit).
  Early design doc for the current implementation. Mostly of historical interest
  only.
