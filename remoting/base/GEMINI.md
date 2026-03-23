# Remoting Base Instructions

Instructions for working in `//remoting/base`.

## Architecture Overview

The `base` directory contains foundational utilities, common data structures,
and abstractions used across all of Chrome Remote Desktop (CRD). Code here
should **not** depend on higher-level components like `host` or `client`.

### Core Components

*   **Authentication & OAuth:**
    *   Contains implementations for retrieving and managing OAuth tokens
        (`OAuthTokenGetter`, `GaiaOAuthClient`) and handling session
        authorization (`CorpSessionAuthzServiceClient`,
        `CloudSessionAuthzServiceClient`).

*   **Protobuf HTTP Client (API Calls):**
    *   CRD uses a custom suite of classes (`ProtobufHttpClient`,
        `ProtobufHttpRequest`, `ProtobufHttpStreamRequest`) to make REST-like
        calls to Google backends (FTL, Directory, etc.) using Protocol Buffers
        over HTTP.
    *   This provides gRPC-like functionality (both unary and streaming) without
        relying on the full gRPC library.
    *   Requests are configured via `ProtobufHttpRequestConfig`, which handles
        URL paths, authentication requirements, and optional `RetryPolicy`.

*   **Crash Reporting (`//remoting/base/crash`):**
    *   **Linux:** Fully migrated to `Crashpad`.
    *   **Windows:** Currently uses `Breakpad` (including an out-of-process
        crash server for low-privilege processes), but is in the process of
        migrating to `Crashpad`.
    *   **Mac:** Crash reporting is currently not implemented in this layer.

*   **Internal Headers & Buildflags:**
    *   `remoting/base/internal_headers.h` abstracts the differences between
        public Chromium builds and Google-internal builds (which have access to
        `//remoting/internal`).
    *   **Mandate:** Always use the wrapper structs and helper functions defined
        in this header (or the stubs in `remoting/proto/internal_stubs.h`) to
        ensure the code builds in both environments.

*   **Threading & Tasks (AutoThread):**
    *   CRD relies heavily on **thread affinity** via `AutoThread` and
        `AutoThreadTaskRunner`.
    *   An `AutoThread` manages its own lifetime; it stays alive as long as
        there are active references to its `AutoThreadTaskRunner` and
        automatically joins/destroys itself when the last reference is dropped.
    *   **Task Utilities:** Use `remoting/base/task_util.h` for helpers like
        `PostWithCallback` and `WrapCallbackToCurrentSequence` to ensure
        asynchronous results are returned to the correct sequence.

*   **Async Code Design:** When adding async code, consider whether the task
    requires a dedicated thread (for performance or OS API reasons) or if it
    can run on a sequence and share CPU time.
    *   **SequenceBound:** Prefer using `base::SequenceBound` (from
        `base/threading/sequence_bound.h`) when an object must live and be
        accessed on a specific sequence/thread. It ensures type-safety and
        prevents accidental cross-thread access.

*   **Logging:**
    *   **Mandate:** Use `HOST_LOG` and `HOST_DLOG` (defined in `logging.h`)
        instead of `LOG(INFO)`. This ensures logs are routed correctly in host
        processes and bypasses "spammy logging" presubmit checks.

*   **Settings & Policies:**
    *   `HostSettings` and `UserSettings` provide platform-specific persistent
        storage (Windows Registry vs. JSON files on Linux/Mac).
    *   `SessionPolicies` encapsulates enterprise policies (e.g.,
        `RemoteAccessHostClipboardSizeBytes`) that must be respected during a
        connection.

*   **Data Structures & Utilities:**
    *   `remoting::Result`: A success/error wrapper (moving toward
        `base::expected`).
    *   `CompoundBuffer`: Optimized for handling fragmented data (e.g., video
        frames) to minimize copying.

## Key Files to Read

*   `remoting/base/auto_thread.h`: The foundation of CRD's threading model.
*   `remoting/base/auto_thread_task_runner.h`: Threading abstractions.
*   `remoting/base/internal_headers.h`: The bridge between public and internal
    builds.
*   `remoting/base/oauth_token_getter.h`: Base interface for OAuth.
*   `remoting/base/protobuf_http_client.h`: The primary engine for backend API
    requests.
*   `remoting/base/logging.h`: Macros for host-side logging.
*   `remoting/base/task_util.h`: Helpers for cross-sequence task posting.
*   `remoting/base/result.h`: Standardized success/error handling type.
