# //media/remoting

This folder provides an interface to play local content on remote devices.

All of the command and control messages are sent over the wire to the end point
as serialized RPC protobuf messages, using the
[remoting.proto](https://chromium.googlesource.com/openscreen/+/HEAD/cast/streaming/remoting.proto)
definition provided by libcast (in Open Screen).

Remoting session negotiation is managed by libcast, however managing the
actual media stack is an implementation detail of Chromium's implementation, so
is located in this folder.
