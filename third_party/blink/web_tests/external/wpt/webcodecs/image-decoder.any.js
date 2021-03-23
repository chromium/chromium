// META: global=window,dedicatedworker
// META: script=/webcodecs/image-decoder-utils.js

function testFourColorsDecode(filename, mimeType) {
  return fetch(filename)
      .then(response => {
        let decoder = new ImageDecoder({data: response.body, type: mimeType});
        return decoder.decode();
      })
      .then(result => {
        assert_equals(result.image.displayWidth, 320);
        assert_equals(result.image.displayHeight, 240);

        let canvas = new OffscreenCanvas(
            result.image.displayWidth, result.image.displayHeight);
        let ctx = canvas.getContext('2d');
        ctx.drawImage(result.image, 0, 0);

        let top_left = toUInt32(ctx.getImageData(0, 0, 1, 1));
        assert_equals(top_left, 0xFFFF00FF, 'top left corner is yellow');

        let top_right =
            toUInt32(ctx.getImageData(result.image.displayWidth - 1, 0, 1, 1));
        assert_equals(top_right, 0xFF0000FF, 'top right corner is red');

        let bottom_left =
            toUInt32(ctx.getImageData(0, result.image.displayHeight - 1, 1, 1));
        assert_equals(bottom_left, 0x0000FFFF, 'bottom left corner is blue');

        let left_corner = toUInt32(ctx.getImageData(
            result.image.displayWidth - 1, result.image.displayHeight - 1, 1,
            1));
        assert_equals(left_corner, 0x00FF00FF, 'bottom right corner is green');
      });
}

promise_test(t => {
  return testFourColorsDecode('four-colors.jpg', 'image/jpeg');
}, 'Test JPEG image decoding.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(1);
}, 'Test JPEG w/ EXIF orientation top-left.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(2);
}, 'Test JPEG w/ EXIF orientation top-right.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(3);
}, 'Test JPEG w/ EXIF orientation bottom-right.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(4);
}, 'Test JPEG w/ EXIF orientation bottom-left.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(5);
}, 'Test JPEG w/ EXIF orientation left-top.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(6);
}, 'Test JPEG w/ EXIF orientation right-top.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(7);
}, 'Test JPEG w/ EXIF orientation right-bottom.');

promise_test(t => {
  return testFourColorDecodeWithExifOrientation(8);
}, 'Test JPEG w/ EXIF orientation left-bottom.');

promise_test(t => {
  return testFourColorsDecode('four-colors.png', 'image/png');
}, 'Test PNG image decoding.');

promise_test(t => {
  return testFourColorsDecode('four-colors.avif', 'image/avif');
}, 'Test AVIF image decoding.');

promise_test(t => {
  return testFourColorsDecode('four-colors.webp', 'image/webp');
}, 'Test WEBP image decoding.');

promise_test(t => {
  return testFourColorsDecode('four-colors.gif', 'image/gif');
}, 'Test GIF image decoding.');

promise_test(t => {
  return fetch('four-colors.png').then(response => {
    let decoder = new ImageDecoder({data: response.body, type: 'junk/type'});
    return promise_rejects_dom(t, 'NotSupportedError', decoder.decode());
  });
}, 'Test invalid mime type rejects decode() requests');

promise_test(t => {
  return fetch('four-colors.png').then(response => {
    let decoder = new ImageDecoder({data: response.body, type: 'junk/type'});
    return promise_rejects_dom(
        t, 'NotSupportedError', decoder.decodeMetadata());
  });
}, 'Test invalid mime type rejects decodeMetadata() requests');

class InfiniteGifSource {
  async load(repetitionCount) {
    let response = await fetch('four-colors-flip.gif');
    let buffer = await response.arrayBuffer();

    // Strip GIF trailer (0x3B) so we can continue to append frames.
    this.baseImage = new Uint8Array(buffer.slice(0, buffer.byteLength - 1));
    this.baseImage[0x23] = repetitionCount;
    this.counter = 0;
  }

  start(controller) {
    this.controller = controller;
    this.controller.enqueue(this.baseImage);
  }

  close() {
    this.controller.enqueue(new Uint8Array([0x3B]));
    this.controller.close();
  }

  addFrame() {
    const FRAME1_START = 0x26;
    const FRAME2_START = 0x553;

    if (this.counter++ % 2 == 0)
      this.controller.enqueue(this.baseImage.slice(FRAME1_START, FRAME2_START));
    else
      this.controller.enqueue(this.baseImage.slice(FRAME2_START));
  }
}

promise_test(async t => {
  let source = new InfiniteGifSource();
  await source.load(5);

  let stream = new ReadableStream(source, {type: 'bytes'});
  let decoder = new ImageDecoder({data: stream, type: 'image/gif'});
  return decoder.decodeMetadata()
      .then(_ => {
        assert_equals(decoder.frameCount, 2);
        assert_equals(decoder.repetitionCount, 5);

        source.addFrame();
        return decoder.decode({frameIndex: 2});
      })
      .then(result => {
        assert_equals(decoder.frameCount, 3);
        assert_equals(result.image.displayWidth, 320);
        assert_equals(result.image.displayHeight, 240);
        source.addFrame();
        return decoder.decode({frameIndex: 3});
      })
      .then(result => {
        assert_equals(decoder.frameCount, 4);
        assert_equals(result.image.displayWidth, 320);
        assert_equals(result.image.displayHeight, 240);

        // Decode frame not yet available then reset before it comes in.
        let p = decoder.decode({frameIndex: 5});
        decoder.reset();
        return promise_rejects_dom(t, 'AbortError', p);
      })
      .then(_ => {
        // Ensure we can still decode earlier frames.
        assert_equals(decoder.frameCount, 4);
        return decoder.decode({frameIndex: 3});
      })
      .then(result => {
        assert_equals(decoder.frameCount, 4);
        assert_equals(result.image.displayWidth, 320);
        assert_equals(result.image.displayHeight, 240);

        // Decode frame not yet available then close before it comes in.
        let p = decoder.decode({frameIndex: 5});
        decoder.close();

        assert_equals(decoder.frameCount, 0);
        assert_equals(decoder.repetitionCount, 0);
        assert_equals(decoder.type, "");
        assert_equals(decoder.tracks.length, 0);

        assert_throws_dom('InvalidStateError', _ => {
          decoder.selectTrack(1);
        });

        // Previous decode should be aborted.
        return promise_rejects_dom(t, 'AbortError', p);
      })
      .then(_ => {
        // Ensure feeding the source after closing doesn't crash.
        source.addFrame();
        return promise_rejects_dom(
            t, 'InvalidStateError', decoder.decodeMetadata());
      })
      .then(_ => {
        return promise_rejects_dom(t, 'InvalidStateError', decoder.decode());
      });
}, 'Test ReadableStream of gif');
