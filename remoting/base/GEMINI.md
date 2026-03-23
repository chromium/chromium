# Remoting Base Instructions

Instructions for working in `//remoting/base`.

## Architecture Overview

The `base` directory contains foundational utilities, common data structures,
and abstractions used across all of Chrome Remote Desktop (CRD). Code here
should **not** depend on higher-level components like `host` or `client`.

### Core Components

*   **Authentication & OAuth:** Contains implementations for retrieving and
    managing OAuth tokens (`OAuthTokenGetter`, `GaiaOAuthClient`) and handling
    session authorization (`CorpSessionAuthzServiceClient`,
    `CloudSessionAuthzServiceClient`).
*   **Protobuf HTTP Client:** CRD uses a custom `ProtobufHttpClient` to make
    REST/gRPC-like calls to Google backends using Protocol Buffers over HTTP.
*   **Threading & Tasks (AutoThread & Thread Affinity):** CRD was originally built
    with strict **thread affinity** rather than the modern Chromium standard of
    **sequence affinity** (`base::SequencedTaskRunner`). This is a pervasive and
    critical pattern. We use `AutoThread` and `AutoThreadTaskRunner` to manage
    thread lifetimes. An `AutoThread` stays alive as long as there are active
    references to its `AutoThreadTaskRunner`. When the last reference is dropped,
    the thread automatically joins and cleans itself up.

*   **Async Code Design:** When adding async code, consider whether the task
    requires a dedicated thread (for performance or OS API reasons) or if it
    can run on a sequence and share CPU time.
    *   **SequenceBound:** Prefer using `base::SequenceBound` (from
        `base/threading/sequence_bound.h`) when an object must live and be
        accessed on a specific sequence/thread. It ensures type-safety and
        prevents accidental cross-thread access.
*   **Settings & Policies:** Classes to manage host and user settings
    (`HostSettings`, `UserSettings`) and session-specific enterprise policies
    (`SessionPolicies`).
*   **Utilities:** General purpose helpers for error handling (`Result`,
    `Errors`), socket reading/writing (`SocketReader`, `BufferedSocketWriter`),
    and string/buffer manipulation (`CompoundBuffer`, `TypedBuffer`).

## Key Files to Read

*   `remoting/base/auto_thread_task_runner.h`: Threading abstractions.
*   `remoting/base/oauth_token_getter.h`: Base interface for OAuth.
*   `remoting/base/protobuf_http_client.h`: Used for backend API requests.
*   `remoting/base/result.h`: Standardized success/error handling type.

