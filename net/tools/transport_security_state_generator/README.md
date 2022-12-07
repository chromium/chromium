# Transport Security State Generator

This directory contains the code for the transport security state generator, a
tool that generates a C++ file based on preload data in
[transport_security_state_static.json](/net/http/transport_security_state_static.json).
This JSON file contains the domain security policy configurations for all
preloaded domains.

[TOC]

## Domain Security Policies

Website owners can set a number of security policies for their domains, usually
by sending configuration in a HTTP header. Chromium supports preloading for some
of these security policies so that users benefit from these policies regardless
of their browsing history. Website owners can request preloading for their
domains. Chromium supports preloading for the following domain security
policies:

* [HTTP Strict Transport Security (HSTS)](https://tools.ietf.org/html/rfc6797)
* [Public Key Pinning Extension for HTTP](https://tools.ietf.org/html/rfc7469)
k

Chromium and most other browsers ship the preloaded configurations inside their
binary. Chromium uses a custom data structure for this.

### I want to preload a website

Please follow the instructions at [hstspreload.org](https://hstspreload.org/).

## I want to use the preload list for another project

Please contact [the list maintainers](https://hstspreload.org/#contact) before
you do.

## The Preload Generator

The transport security state generator is executed during the build process (it
may execute multiple times depending on the targets you're building) and
generates data structures that are compiled into the binary. You can find the
generated output in
`[build-folder]/gen/net/http/transport_security_state_static*.h`.

### Usage

Make sure you have build the `transport_security_state_generator` target.

`transport_security_state_generator <json-file> <pins-file> <template-file> <output-file> [--v=1]`

*  **json-file**: JSON file containing all preload configurations (e.g.
   `net/http/transport_security_state_static.json`)
*  **pins-file**: file containing the public key information for the pinsets
   referenced from **json-file** (e.g.
   `net/http/transport_security_state_static.pins`)
*  **template-file**: contains the global structure of the header file with
   placeholder for the generated data (e.g.
   `net/http/transport_security_state_static.template`)
*  **output-file**: file to write the output to
*  **--v**: verbosity level

## The Preload Format

The preload data is stored in the Chromium binary as a trie encoded in a byte
array (`net::TransportSecurityStateSource::preloaded_data`). The hostnames are
stored in their canonicalized form and compressed using a Huffman coding. The
generic decoder for preloaded Huffman encoded trie data is `PreloadDecoder` and
lives in `net/extras/preload_data/decoder.cc`. The HSTS specific implementation
is `DecodeHSTSPreload` and lives in `net/http/transport_security_state.cc`.

### Huffman Coding

A Huffman coding is calculated for all characters used in the trie (characters
in hostnames and the `end of table` and `terminal` values). The Huffman tree
can be rebuild from the `net::TransportSecurityStateSource::huffman_tree`
array.

The (internal) nodes of the tree are encoded as pairs of uint8s. The last node
in the array is the root of the tree. Each node is two uint8_t values, the first
is "left" and the second is "right". If a uint8_t value has the MSB set it is a
leaf value and the 7 least significant bits represent a ASCII character (from
the range 0-127, the tree does not support extended ASCII). If the MSB is not
set it is a pointer to the n'th node in the array.

For example, the following uint8_t array

`0xE1, 0xE2, 0xE3, 0x0, 0xE4, 0xE5, 0x1, 0x2`

represents 9 elements:

*  the implicit root node (node 3)
*  3 internal nodes: 0x0 (node 0), 0x1 (node 1), and 0x2 (node 2)
*  5 leaf values: 0xE1, 0xE2, 0xE3, 0xE4, and 0xE5 (which all have the most
significant bit set)

When decoded this results in the following Huffman tree:

```
             root (node 3)
            /             \
     node 1                 node 2
   /       \               /      \
0xE3 (c)    node 0     0xE4 (d)    0xE5 (e)
           /      \
       0xE1 (a)    0xE2 (b)
```


### The Trie Encoding

The byte array containing the trie is made up of a set of nodes represented by
dispatch tables. Each dispatch table contains a (possibly empty) shared prefix,
a value, and zero or more pointers to child dispatch tables. The node value
is an encoded entry and the associated hostname can be found by going up the
trie.

The trie contains the hostnames in reverse and the hostnames are terminated by a
`terminal value`.

The dispatch table for the root node starts at bit position
`net::TransportSecurityStateSource::root_position`.

The binary format for the trie is defined by the following
[ABNF](https://tools.ietf.org/html/rfc5234).

```abnf
trie               = 1*dispatch-table

dispatch-table     = prefix-part         ; a common prefix for the node and its children
                     1*value-part        ; 1 or more values or pointers to children
                     end-of-table-value  ; signals the end of the table

prefix-part        = prefix-length      ; a prefix code encoding of the number
of characters in the prefix
                     prefix-characters  ; the actual prefix characters
prefix-length      = 1*BIT  ; See net::extras::PreloadDecoder::DecodeSize for the format
value-part         = huffman-character node-value
                     ; table with the node value and pointers to children

node-value         = node-entry    ; preload entry for the hostname at this node
                   / node-pointer  ; a bit offset pointing to another dispatch
                                   ; table

node-entry         = preloaded-entry  ; encoded preload configuration for one
                                      ; hostname (see section below)
node-pointer       = long-bit-offset
                   / short-bit-offset

long-bit-offset    = %b1      ; 1 bit indicates long form will follow
                     4BIT     ; 4 bit number indicating bit length of the offset
                     8*22BIT  ; offset encoded as an n bit number (see above)
                              ; where n is the offset length (see above) + 8
short-bit-offset   = %b0      ; 0 bit indicates short form will follow
                     7BIT     ; offset as a 7 bit number

terminal-value     = huffman-character  ; ASCII value 0x00 encoded using Huffman
end-of-table-value = huffman-character  ; ASCII value 0x7F encoded using Huffman

prefix-characters  = *huffman-character
huffman-character  = 1*BIT
```

### The Preloaded Entry Encoding

The entries are encoded using a variable length encoding. Each entry is made up
of 4 parts, one for each supported policy. The length of these parts depends
on the actual configuration, some field will be omitted in some cases.

The binary format for an entry is defined by the following ABNF.

```abnf
preloaded-entry    = BIT                   ; simple entry flag
                     [hsts-part hpkp-part]
                                           ; policy specific parts are only
                                           ; present when the simple entry flag
                                           ; is set to 0 and omitted otherwise

hsts-part          = include-subdomains    ; HSTS includeSubdomains flag
                     BIT                   ; whether to force HTTPS

hpkp-part          = BIT                   ; whether to enable pinning
                     [pinset-id]           ; only present when pinning is enabled
                     [include-subdomains]  ; HPKP includeSubdomains flag, only
                                           ; present when pinning is enabled and
                                           ; HSTS includeSubdomains is not used

hpkp-pinset-id     = array-index

report-uri-id      = array-index
include-subdomains = BIT
array-index        = 4BIT             ; a 4 bit number
```

The **array-index** values are indices in the associated arrays:

*  `net::TransportSecurityStateSource::pinsets` for **pinset-id**
**report-uri-id**

#### Simple entries

The majority of entries on the preload list are submitted through
[hstspreload.org](https://hstspreload.org) and share the same policy
configuration (HSTS + includeSubdomains only). To safe space, these entries
(called **simple entries**) use a shorter encoding where the first bit (simple
entry flag) is set to 1 and the rest of the configuration is omitted.

### Tests

The generator code has its own unittests in the
`net/tools/transport_security_state_generator` folder.

The encoder and decoder for the preload format life in different places and are
tested by end-to-end tests (`TransportSecurityStateTest.DecodePreload*`) in
`net/http/transport_security_state_unittest.cc`. The tests use their own
preload lists, the data structures for these lists are generated in the same way
as for the official Chromium list.

All these tests are part of the `net_unittests` target.

#### Writing tests that depend on static transport security state

Tests in `net_unittests` (except for `TransportSecurityStateStaticTest`) should
not depend on the real preload list. If you are writing tests that require a
static transport security state use
`transport_security_state_static_unittest_default.json` instead. Tests can
override the active preload list by calling
`SetTransportSecurityStateSourceForTesting`.

## See also

* <https://hstspreload.org/>
* <https://www.chromium.org/hsts>
