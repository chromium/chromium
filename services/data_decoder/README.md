The data_decoder service exists to facilitate safe data decoding within an
isolated sandboxed process.

NOTE: Protobuf is considered robust enough to decode untrusted input even
without sandboxing. So you won't find a protobuf decoder in this service.
