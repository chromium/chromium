# Compression dictionary transport with Shared Brotli

This directory contains the implementation for Compression Dictionary Transport
with Shared Brotli.

## Class overview

When `NetworkContextParams.shared_dictionary_enabled` flag is enabled, a
`SharedDictionaryManager` is attached to a `NetworkContext`.
The `SharedDictionaryManager` manages `SharedDictionaryStorage`, which is
created per `net::NetworkIsolationKey`. `SharedDictionaryStorage` manages
compression dictionaries, also known as shared dictionaries.

We have two implementations of `SharedDictionaryManager`,
`SharedDictionaryStorage`, `SharedDictionaryWriter` and `SharedDictionary`.
Classes with an "InMemory" suffix in their name are for incognito mode. And
classes with an "OnDisk" suffix are for normal Profiles.
(Note: We are currently actively implementing "OnDisk" classes.)

## Storing dictionaries

When `CorsURLLoader` receives a HTTP response, it calls
`SharedDictionaryStorage::MaybeCreateWriter()`. If the received header contans
an appropriate `'use-as-dictionary'` header, this method returns a
`SharedDictionaryWriter`. `CorsURLLoader` then creates a
`SharedDictionaryDataPipeWriter` to write the response body to the storege
via the `SharedDictionaryWriter`.

`SharedDictionaryWriterInMemory` just copies the response body to the memory.
`SharedDictionaryWriterOnDisk` writes the response body to the disk cache using
`SharedDictionaryDiskCache` class.

When `SharedDictionaryWriter` finishes writing the body,
`SharedDictionaryStorage::OnDictionaryWritten` will be called.
`SharedDictionaryStorageInMemory` just keeps the dictionary information in the
memory. `SharedDictionaryStorageOnDisk` will stores the dictionary information
to the storage database (Note: Not implemented yet).

### Limitations

We currently set a size limit of 100 MiB per dictionary. This is intended to
protect the network services from out-of-memory denial-of-service attacks.

## Flags

The feature of Compression dictionary transport with Shared Brotli is currently
controlled by two flags.

1. CompressionDictionaryTransportBackend
    - Users can enable/disable using
      chrome://flags/#enable-compression-dictionary-transport-backend.
    - This feature will be enabled/disabled by Field Trial Config.
    - This will be used for kill-switch. All feature must be disabled
      when this feature is disabled.
    - When this feature is enabled, the network service will check the
      storage of dictionaries while feching resources.

2. CompressionDictionaryTransport
    - Users can enable/disable using
      chrome://flags/#enable-compression-dictionary-transport
    - This feature can be enabled by Origin Trial.
    - This feature can also be enabled/disabled by Field Trial Config.
    - When both the backend feature and this feature are enabled:
      - The network service will store HTTP responses with
        "use-as-dictionary" response header to the dictionary storage.
        (Note: Not implemented yet.)
      - Blink will fetch the dictionary after detecting
        `<link rel=compression-dictionary>` in the document HTML or
        "`Link: rel=compression-dictionary`" in the HTTP response header.
        (Note: Not implemented yet.)
      - HTMLLinkElement.relList.supports('dictionary') will return true.
        (Note: Not implemented yet.)
    - Note: Until M126, `rel=dictionary` was used instead of
      `rel=compression-dictionary`.

## Links

- [Explainer](https://github.com/WICG/compression-dictionary-transport)
- [Crbug](httpe://crbug.com/1413922)
- [Chrome Status](https://chromestatus.com/feature/5124977788977152)
- [Design doc](https://docs.google.com/document/d/1IcRHLv-e9boECgPA5J4t8NDv9FPHDGgn0C12kfBgANg/edit?usp=sharing)
