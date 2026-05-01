# Remoting Host Mojo Interfaces

This directory defines the Mojo interfaces for IPC between CRD host processes.

## Process Communication Map

Understanding which processes use which interfaces is key to navigating the host
architecture.

*   **`remoting_host.mojom`**
    *   *Users:* `Network` <-> `Daemon`
    *   *Role:* High-level host control and status.
*   **`desktop_session.mojom`**
    *   *Users:* `Network` <-> `Desktop`
    *   *Role:* Screen capture, input injection, and session lifecycle.
*   **`chromoting_host_services.mojom`**
    *   *Users:* `Network` <-> `UserProcess`
    *   *Role:* Allows user-level processes (like remote-open-url) to request
        services.
*   **`remote_url_opener.mojom`**
    *   *Users:* `Network` <-> `RemoteOpenUrl`
    *   *Role:* URL forwarding from client to host.
*   **`webauthn_proxy.mojom`**
    *   *Users:* `Network` <-> `WebAuthn`
    *   *Role:* WebAuthn request/response proxying.
*   **`login_session.mojom`**
    *   *Users:* `Daemon` <-> `LoginSession`
    *   *Role:* Linux-specific session login management.

## Important Guidelines

1.  **Security Review:** Any changes to Mojo interfaces that span process
    boundaries MUST be reviewed for security.
2.  **Versioning:** All binaries are versioned together so it is OK to change
    the mojom in ways which would constitute a breaking change otherwise.
3.  **Broker Process:** The `Daemon` process is typically the Mojo Broker. All
    initial Mojo connections should be established through it.
4.  **Async Design:** Use `OnceCallback` and `Remote` carefully. Most host IPC
    is asynchronous to prevent blocking the network thread.

## How to Extend

To add a new process type:

1.  Define a new `.mojom` interface here.
2.  Update `chromoting_host_services.mojom` if the new process needs to be
    accessible from other user processes.
3.  Implement the interface in the appropriate `remoting/host/` implementation
    file.

## Unit Testing Traits

When adding or modifying `StructTraits`, `EnumTraits`, or `UnionTraits` in
`remoting_mojom_traits.h`, you MUST add or update the corresponding tests in
`remoting_mojom_traits_unittest.cc`. These tests ensure that the mapping between
C++ types and Mojom types is correct and remains stable.

Example test pattern:
```cpp
TEST(RemotingMojomTraitsTest, MyType) {
  MyType input = ...;
  MyType output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::MyType>(input, output));
  EXPECT_EQ(input, output);
}
```
