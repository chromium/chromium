Mojo Proxy
====
The Mojo Proxy is a simple standalone service which can act as a bridge between
a process using Mojo+ipcz and a process running only the legacy Mojo Core
implementation. Currently the service is only supported on POSIX systems.

Usage
----
On POSIX systems, a host process normally sends an invitation to some other
client or service (the "target" herein) via a Unix socket. The invitation has
one or more pipe attachments associated with it. The target accepts this
invitation from a peer socket and extracts its pipe attachments, thus
bootstrapping Mojo IPC for the target:

```c++
mojo::ScopedMessagePipeHandle ConnectToTarget(
    mojo::PlatformChannelEndpoint endpoint) {
  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe("pipe!");
  mojo::OutgoingInvitation::Send(std::move(invitation), {},
                                 std::move(endpoint));
  return pipe;
}
```

If the host process is running Mojo+ipcz but the target is not, the host can
instead launch an instance of this proxy executable to act as an intermediary.

This will require the host to allocate an additional socket pair for
communication with the proxy.

Also note the proxy should be launched from the host via fork+exec so that it
can inherit the necessary file descriptors.

```c++
mojo::ScopedMessagePipeHandle ConnectToLegacyTarget(
    mojo::PlatformChannelEndpoint endpoint) {
  // Create a new channel for host<->proxy communication.
  mojo::PlatformChannel proxy_channel;

  // Somehow launch the `mojo_proxy` instance via fork+exec, inheriting the
  // two passed descriptors. `target_fd` must be identified by
  // `--legacy-client-fd=N` and `proxy_fd` must be identified by
  // `--host-ipcz-transport-fd=N` on the `mojo_proxy` command line.
  // The attachment name "pipe!" must be passed via `--attachment-name`.
  base::ScopedFD target_fd = endpoint.TakePlatformHandle().TakeFD();
  base::ScopedFD proxy_fd =
      proxy_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
  LaunchProxy(std::move(target_fd), std::move(proxy_fd), "pipe!");

  // Otherwise invitation looks the same as before, except we're sending it
  // over a different endpoint which goes through the proxy.
  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe("pipe!");
  mojo::OutgoingInvitation::Send(std::move(invitation), {},
                                 proxy_channel.TakeLocalEndpoint());
  return pipe;
}
```

For applications which attach multiple pipes to an invitation, only numeric
pipe names are supported. They can be passed to the proxy using
`--numeric-attachment-names=0,1,2` instead of using `--attachment-name`.

Finally, if the host is not a broker process it must also pass
`--inherit-ipcz-broker` to the proxy.

