import os.path
import zlib
import gzip

def read(file):
    path = os.path.join(os.path.dirname(__file__), file)
    return open(path, u"rb").read()

def main(request, response):
    response.headers.set(b"Access-Control-Allow-Origin", b"*")
    response.headers.set(b"Content-Type", b"text/plain")
    response.headers.set(
        b"Content-Dictionary",
        b":U5abz16WDg7b8KS93msLPpOB4Vbef1uRzoORYkJw9BY=:")

    # "compressed.br-d.data" is generated using the following commands:
    # $ echo "This is a test dictionary." > /tmp/dict
    # $ echo -n "This is compressed test data using a test dictionary" \
    #    > /tmp/data
    # $ brotli -o compressed.br-d.data -D /tmp/dict /tmp/data
    br_d_filepath = u"./compressed.br-d.data"

    # "compressed.zstd-d.data" is generated using the following commands:
    # $ echo "This is a test dictionary." > /tmp/dict
    # $ echo -n "This is compressed test data using a test dictionary" \
    #    > /tmp/data
    # $ zstd -o compressed.zstd-d.data -D /tmp/dict /tmp/data
    zstd_d_filepath = u"./compressed.zstd-d.data"
    if b'content_encoding' in request.GET:
        content_encoding = request.GET.first(b"content_encoding")
        response.headers.set(b"Content-Encoding", content_encoding)
        if content_encoding == b"br-d":
            # Send the pre compressed file
            response.content = read(br_d_filepath)
        if content_encoding == b"zstd-d":
            # Send the pre compressed file
            response.content = read(zstd_d_filepath)