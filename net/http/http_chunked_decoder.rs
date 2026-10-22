// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[cxx::bridge(namespace = "net::ffi")]
mod ffi {
    extern "Rust" {
        type HttpChunkedDecoder;

        fn new_http_chunked_decoder() -> Box<HttpChunkedDecoder>;
        fn reached_eof(self: &HttpChunkedDecoder) -> bool;
        fn bytes_after_eof(self: &HttpChunkedDecoder) -> i32;
        fn filter_buf(self: &mut HttpChunkedDecoder, buf: &mut [u8]) -> i32;
    }
}

const MAX_LINE_BUF_LEN: usize = 16384;

struct DecodeError;

#[derive(Default, PartialEq, Eq)]
enum State {
    #[default]
    ReadingChunkSize,
    ReadingChunkData(u64),
    ReadingChunkTerminator,
    ReadingTrailers,
    Eof,
}

#[derive(Default)]
pub struct HttpChunkedDecoder {
    state: State,
    line_buf: Vec<u8>,
    bytes_after_eof: i32,
}

fn new_http_chunked_decoder() -> Box<HttpChunkedDecoder> {
    Box::new(HttpChunkedDecoder::default())
}

impl HttpChunkedDecoder {
    fn reached_eof(&self) -> bool {
        self.state == State::Eof
    }

    fn bytes_after_eof(&self) -> i32 {
        self.bytes_after_eof
    }

    fn filter_buf(&mut self, buf: &mut [u8]) -> i32 {
        match self.filter_buf_impl(buf) {
            Ok(n) => i32::try_from(n).unwrap(),
            Err(DecodeError) => -1,
        }
    }

    // Filters out chunk markers in buf, mutating its contents, and returns
    // either the size of the decoded data or an error marker.
    //
    // As a reminder, chunked encoding looks like this: the body is a stream
    // of chunks, each of which is prefixed with its length in octets as a big-
    // endian hex integer and a '\r\n'. The stream of chunks is terminated with
    // a 0-length chunk.
    fn filter_buf_impl(&mut self, mut buf: &mut [u8]) -> Result<usize, DecodeError> {
        let mut result: usize = 0;
        while !buf.is_empty() {
            match self.state {
                State::ReadingChunkData(chunk_remaining) => {
                    let num = usize::try_from(chunk_remaining).unwrap_or(usize::MAX).min(buf.len());
                    let remaining = chunk_remaining - num as u64;
                    result += num;
                    buf = &mut buf[num..];
                    self.state = if remaining == 0 {
                        State::ReadingChunkTerminator
                    } else {
                        State::ReadingChunkData(remaining)
                    };
                    continue;
                }
                State::Eof => {
                    self.bytes_after_eof = self.bytes_after_eof.wrapping_add(buf.len() as i32);
                    break;
                }
                State::ReadingChunkSize
                | State::ReadingChunkTerminator
                | State::ReadingTrailers => {}
            }

            let bytes_consumed = self.scan_for_chunk_remaining(buf)?;
            let subspan_len = buf.len() - bytes_consumed;
            // TODO(crbug.com/575550001): Avoid shifting the remaining buffer on
            // every chunk header/terminator when multiple chunks are present.
            if subspan_len > 0 {
                buf.copy_within(bytes_consumed.., 0);
            }
            buf = &mut buf[..subspan_len];
        }
        Ok(result)
    }

    fn scan_for_chunk_remaining(&mut self, buf: &[u8]) -> Result<usize, DecodeError> {
        let (line, bytes_consumed, has_lf) = match buf.iter().position(|&b| b == b'\n') {
            Some(i) => (&buf[..i], i + 1, true),
            None => (buf, buf.len(), false),
        };
        let line = line.strip_suffix(b"\r").unwrap_or(line);

        let full_line = if !has_lf || !self.line_buf.is_empty() {
            if self.line_buf.len() + line.len() > MAX_LINE_BUF_LEN {
                return Err(DecodeError);
            }
            self.line_buf.extend_from_slice(line);
            if !has_lf {
                return Ok(bytes_consumed);
            }
            self.line_buf.as_slice()
        } else {
            line
        };

        self.state = match self.state {
            State::ReadingTrailers if full_line.is_empty() => State::Eof,
            State::ReadingTrailers => State::ReadingTrailers,
            State::ReadingChunkTerminator if full_line.is_empty() => State::ReadingChunkSize,
            State::ReadingChunkTerminator => return Err(DecodeError),
            State::ReadingChunkSize => match Self::parse_chunk_size(full_line)? {
                0 => State::ReadingTrailers,
                n => State::ReadingChunkData(n),
            },
            State::ReadingChunkData(_) | State::Eof => unreachable!(),
        };
        self.line_buf.clear();
        Ok(bytes_consumed)
    }

    // This is a bit awkward because it is trying to match the behavior of the
    // C++ parser, which accepts any number of hex digits, then any number of
    // spaces. Chunk extensions (which are described in RFC 9112 7.1.1) are
    // ignored; Chrome doesn't support or use any of them.
    fn parse_chunk_size(buf: &[u8]) -> Result<u64, DecodeError> {
        let mut val: u64 = 0;
        let mut has_digits = false;
        let mut trailing_spaces = false;
        // This loop is a simple state machine, driven by individual digits as
        // we parse.
        for &b in buf {
            let digit = match b {
                b';' => break,
                b' ' => {
                    if !has_digits {
                        return Err(DecodeError);
                    }
                    trailing_spaces = true;
                    continue;
                }
                b'0'..=b'9' if !trailing_spaces => b - b'0',
                b'a'..=b'f' if !trailing_spaces => b - b'a' + 10,
                b'A'..=b'F' if !trailing_spaces => b - b'A' + 10,
                _ => return Err(DecodeError),
            };

            // This ugly little thing is so we can ensure that the returned u64
            // fits in an i64; this isn't required by the RFC but it is what
            // the C++ parser does.
            if val > (i64::MAX as u64 >> 4) {
                return Err(DecodeError);
            }
            val = (val << 4) | u64::from(digit);
            has_digits = true;
        }
        if !has_digits {
            return Err(DecodeError);
        }
        Ok(val)
    }
}
