# Isolated Web Apps Component

This directory contains the core, browser-agnostic logic for Isolated Web Apps
(IWAs). It is designed to be independent of high-level browser features like UI
or app management systems. The focus lies on the fundamental building blocks of
the IWA platform.

## Key Responsibilities

*   **Signed Web Bundles:** Logic for parsing `.swbn` files, verifying their
    integrity blocks (signatures and public keys), and extracting metadata. This
    ensures the content of an IWA is authentic and has not been tampered with.
*   **Core Types:** The general IWA data types, such as:
    *   `IwaOrigin`: The unique origin of an IWA, derived from its Signed Web
        Bundle ID.
    *   `IwaVersion`: Handles parsing and comparison of version strings.
    *   `IwaSource`: Where an IWA resource originates from (i.e., a local bundle
        file or a proxy for development).
*   **URL Loading:** The infrastructure to handle the `isolated-app://` scheme.
    It translates requests into reads from the underlying Signed Web Bundle.
*   **Reader Caching:** The `IsolatedWebAppReaderRegistry` maintains a cache of
    bundle readers. This is an optimization to allow repeated requests to reuse
    readers, while managing system resources like file handles and memory.
*   **Embedder Bridge:** The `IwaClient` is the interface between this component
    and the rest of the system (e.g. Chrome's web app code).

## Directory Structure

*   `bundle_operations/`: Orchestrates high-level bundle tasks like ID
    extraction, signature validation, and resource management.
*   `download/`: Logic for downloading Signed Web Bundles, including support for
    partial downloads used for metadata extraction.
*   `error/`: Defines IWA-specific error types (e.g. for unusable bundle files),
    and utilities to log these errors to UMA.
*   `identity/`: Validation of IWA identity and public keys, including support
    for key rotation.
*   `public/`: Interfaces implemented by the embedder to provide essential
    runtime information (e.g. key rotation data).
*   `reading/`: Low-level logic for parsing and validating Signed Web Bundle
    contents, and managing bundle readers.
*   `service/`: Logic around creating and managing keyed services.
*   `test_support/`: Test utilities, including tools to build Signed Web Bundles
    and a `TestIwaClient` to mock embedder interactions.
*   `types/`: Core data types (e.g. `IwaOrigin`, `IwaVersion`, `IwaSource`).
*   `url_loading/`: Implementation of the `isolated-app://` scheme and its
    `URLLoaderFactory` to translate network requests into bundle reads.

