# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Assembles a binary protobuf from a base ImageDecoder fuzzer protobuf and the
contents of the web_tests/images/resources/ directory.
"""

import copy
import os
import sys

# go up 6 parent directories to //src/third_party/blink
path_to_blink = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 *[os.path.pardir] * 4))

# go up 2 parent directories to //src
path_to_src_root = os.path.join(path_to_blink, *[os.path.pardir] * 2)

# allow importing modules from //src/components/resources/protobufs
sys.path.insert(
    0,
    os.path.normpath(
        os.path.join(path_to_src_root, 'components/resources/protobufs')))

from binary_proto_generator import BinaryProtoGenerator

EXTENSIONS_MAP = {
    "avif": "image/avif",
    "png": "image/png",
    "ico": "image/x-icon",
    "bmp": "image/bmp",
    "jpg": "image/jpeg",
    "gif": "image/gif",
    "cur": "image/x-icon",
    "webp": "image/webp",
}


class ImageDecoderProtoGenerator(BinaryProtoGenerator):
    def ImportProtoModule(self):
        import fuzzer_inputs_pb2
        globals()['fuzzer_inputs_pb2'] = fuzzer_inputs_pb2

    def EmptyProtoInstance(self):
        # pylint: disable=undefined-variable
        # `fuzzer_inputs_pb2` is placed directly in `globals()`
        return fuzzer_inputs_pb2.ImageDecoderApiInvocationSequence()

    def ProcessPb(self, opts, pb):
        self._outdir = opts.outdir
        self._processed_pb = pb
        if not os.path.exists(self._outdir):
            os.makedirs(self._outdir)

    def WritePb(self, image_fn, image_type):
        pb = copy.deepcopy(self._processed_pb)
        pb.config.type = image_type
        with open(image_fn, 'rb') as input_image:
            pb.config.data = input_image.read()

        out_fn = os.path.basename(image_fn) + '.pb'
        with open(os.path.join(self._outdir, out_fn), 'wb') as out_file:
            out_file.write(pb.SerializeToString())


def main():
    generator = ImageDecoderProtoGenerator()
    generator.Run()

    image_data_dir = os.path.join(path_to_blink, 'web_tests/images/resources/')
    for root, _, files in os.walk(os.path.join(image_data_dir)):
        for fn in files:
            _, ext = os.path.splitext(fn)
            ext = ext.lower().split('.')[1]
            if ext.lower() in EXTENSIONS_MAP:
                generator.WritePb(os.path.join(root, fn),
                                  EXTENSIONS_MAP[ext.lower()])


if __name__ == '__main__':
    sys.exit(main())
