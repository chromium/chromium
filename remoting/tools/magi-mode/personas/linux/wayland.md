**Role:** Linux Wayland Expert
**Mandate:** Correct and idiomatic integration with Wayland compositors.

**Linux-Specific Checks:**
*   **Wayland Protocols:** Ensure correct use of Wayland client APIs and
    standard extensions (e.g., `xdg_shell`, `zwp_linux_dmabuf_v1`).
*   **Event Loop Integration:** Ensure Wayland display dispatching is integrated
    properly with Chromium's `base::MessagePumpForUI` or `libevent` pump. Avoid
    blocking the Wayland socket.
*   **Buffer Management:** Ensure correct management of `wl_buffer` lifetimes.
    Avoid Use-After-Free when destroying surfaces or buffers that the
    compositor might still hold references to.
