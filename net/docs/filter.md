The network stack implements support for Content-Encodings using
"source streams", which can be composed together and mutate all the incoming
bytes from a URLRequestJob. Currently, the following streams are implemented:

* gzip (handling "deflate" and "gzip" Content-Encodings)
* brotli (handling "br" Content-Encoding)
* zstd (handling "zstd" Content-Encoding)

Source streams conceptually form a chain, with the URLRequestJob as both the
beginning and end of the chain, meaning the URLRequestJob produces raw bytes at
the end and consumes unencoded bytes at the beginning. For example, to support a
hypothetical "Content-Encoding: bar,foo", streams would be arranged like so,
with "X <-- Y" meaning "data flows from Y to X", "X reads data from Y", or
"X is downstream from Y".

  URLRequestJob <-- BarSourceStream <-- FooSourceStream <-- URLRequestJob
                                                       (URLRequestSourceStream)

Here URLRequestJob pulls filtered bytes from its upstream, BarSourceStream,
which pulls filtered bytes from FooSourceStream, which in turn pulls raw bytes
from URLRequestJob. URLRequestSourceStream is the ultimate upstream, which
produces the raw data. Every stream in the chain owns its upstream. In this
case, the ultimate downstream, URLRequestJob, owns its upstream,
BarSourceStream, which in turn owns FooSourceStream. FooSourceStream owns its
upstream, URLRequestSourceStream.

All source streams conform to the following interface (named SourceStream in the
tree):

  int Read(IOBuffer* dest_buffer, size_t buffer_size,
           const OnReadCompleteCallback& callback);

This function can return either synchronously or asynchronously via the supplied
callback. The source stream chain is "pull-based", in that data does not
propagate through the chain until requested by the final consumer of the
filtered data.

Shared dictionary decompression for encodings "dcb" and "dcz" is handled by a
different mechanism. See the SharedDictionaryNetworkTransaction class.
