# QUIC platform

This platform/ directory exists in order to allow QUIC code to be built on
numerous platforms. It contains two subdirectories:

-   api/ contains platform independent class definitions for fundamental data
    structures (e.g., IPAddress, SocketAddress, etc.).
-   impl/ contains platform specific implementations of these data structures.
    The content of files in impl/ will vary depending on the platform.

Code in the parent quic/ directory should not depend on any platform specific
code, other than that found in impl/.
