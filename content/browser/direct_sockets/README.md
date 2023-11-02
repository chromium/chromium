This directory implements permission and user consent checks for the
browser side of socket opening for the [Direct Sockets API](
https://github.com/WICG/direct-sockets/blob/main/docs/explainer.md).

Examples of the checks include
- user dialog, allowing user to enter destination address
- permissions policy
- rate limiting
- checking hostnames resolve to public addresses
- content security policy

When requests to establish TCP or UDP communication have passed the
various checks, they are forwarded to the network service.
