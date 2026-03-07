// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const K_JPEG_MAX_HUFFMAN_CODE_LENGTHS: usize = 16;
const K_JPEG_MAX_HUFFMAN_SYMBOLS: usize = 162;
const K_JPEG_MAX_HUFFMAN_TABLE_NUM_BASELINE: usize = 2;
const K_JPEG_MAX_COMPONENTS: usize = 4;
const K_JPEG_MAX_QUANTIZATION_TABLE_NUM: usize = 4;
const MARKER_SIZE: usize = 2;

#[cxx::bridge(namespace = "media::parsers")]
mod ffi {
    #[derive(Default)]
    struct JpegComponent {
        id: u8,
        horizontal_sampling_factor: u8,
        vertical_sampling_factor: u8,
        quantization_table_selector: u8,
    }

    #[derive(Default)]
    struct JpegFrameHeader {
        visible_width: u16,
        visible_height: u16,
        coded_width: u16,
        coded_height: u16,
        num_components: u8,
        components: Vec<JpegComponent>,
    }

    #[derive(Default)]
    struct JpegScanComponent {
        component_selector: u8,
        dc_selector: u8,
        ac_selector: u8,
    }

    #[derive(Default)]
    struct JpegScanHeader {
        num_components: u8,
        components: Vec<JpegScanComponent>,
    }

    struct JpegHuffmanTable {
        valid: bool,
        code_length: [u8; 16], // cxx requires literals for array sizes
        code_value: [u8; 162],
    }

    struct JpegQuantizationTable {
        valid: bool,
        // baseline only supports 8 bits quantization table
        value: [u8; 64],
    }

    enum JpegParserError {
        Ok = 0,
        IoError = 1,
        UnexpectedEndOfSegment = 2,
        Only8BitPrecisionSupported = 3,
        UnsupportedNumberOfComponents = 4,
        InvalidComponentId = 5,
        InvalidHorizontalSamplingFactor = 6,
        InvalidVerticalSamplingFactor = 7,
        InvalidQuantizationTablePrecision = 8,
        Invalid16BitQuantizationTable = 9,
        InvalidQuantizationTableId = 10,
        InvalidHuffmanTableClass = 11,
        InvalidHuffmanTableId = 12,
        TooManyHuffmanCodes = 13,
        InvalidDriSegmentSize = 14,
        ScanComponentCountMismatch = 15,
        ComponentSelectorMismatch = 16,
        InvalidDcSelector = 17,
        InvalidAcSelector = 18,
        UnsupportedSpectralSelection = 19,
        UnsupportedPointTransform = 20,
        InvalidMarkerSegmentSize = 21,
        UnexpectedEndOfImage = 22,
        EoiMarkerNotFound = 23,
        SoiMarkerNotFound = 24,
        ExpectedMarkerPrefix = 25,
        OnlySof0Supported = 26,
        DqtMarkerNotFound = 27,
    }

    struct JpegParseResult {
        error_code: JpegParserError,
        frame_header: JpegFrameHeader,

        dc_table: [JpegHuffmanTable; 2],
        ac_table: [JpegHuffmanTable; 2],
        q_table: [JpegQuantizationTable; 4],

        restart_interval: u16,
        scan: JpegScanHeader,

        data_offset: usize,
        data_length: usize,
        // The size of the first entire image including header.
        image_size: usize,
    }

    extern "Rust" {
        fn parse_jpeg_picture_ffi(data: &[u8]) -> JpegParseResult;
    }
}

use ffi::{
    JpegComponent, JpegFrameHeader, JpegHuffmanTable, JpegParseResult, JpegParserError,
    JpegQuantizationTable, JpegScanComponent, JpegScanHeader,
};
use std::io::{BufRead, Cursor, Read};

mod jpeg_marker {
    pub const SOF0: u8 = 0xC0;
    pub const SOF1: u8 = 0xC1;
    pub const SOF2: u8 = 0xC2;
    pub const SOF3: u8 = 0xC3;
    pub const DHT: u8 = 0xC4;
    pub const SOF5: u8 = 0xC5;
    pub const SOF6: u8 = 0xC6;
    pub const SOF7: u8 = 0xC7;
    pub const SOF9: u8 = 0xC9;
    pub const SOF10: u8 = 0xCA;
    pub const SOF11: u8 = 0xCB;
    pub const SOF13: u8 = 0xCD;
    pub const SOF14: u8 = 0xCE;
    pub const SOF15: u8 = 0xCF;
    pub const RST0: u8 = 0xD0;
    pub const RST7: u8 = 0xD7;
    pub const SOI: u8 = 0xD8;
    pub const EOI: u8 = 0xD9;
    pub const SOS: u8 = 0xDA;
    pub const DQT: u8 = 0xDB;
    pub const DRI: u8 = 0xDD;
    pub const MARKER_PREFIX: u8 = 0xFF;
}

/// Returns true if there is at least one byte remaining in the cursor.
fn has_data_left(cursor: &mut Cursor<&[u8]>) -> Result<bool, JpegParserError> {
    Ok(cursor.position() < (cursor.get_ref().len() as u64))
}

/// Reads a single byte from the reader.
fn read_u8<R: Read>(reader: &mut R) -> Result<u8, JpegParserError> {
    let mut buf = [0; 1];
    reader.read_exact(&mut buf).map_err(|_| JpegParserError::IoError)?;
    Ok(buf[0])
}

/// Reads a big-endian 16-bit integer from the reader.
fn read_u16<R: Read>(reader: &mut R) -> Result<u16, JpegParserError> {
    let mut buf = [0; 2];
    reader.read_exact(&mut buf).map_err(|_| JpegParserError::IoError)?;
    Ok(u16::from_be_bytes(buf))
}

/// Reads a fixed-length slice of bytes from the cursor.
fn read_slice<'a>(cursor: &mut Cursor<&'a [u8]>, len: usize) -> Result<&'a [u8], JpegParserError> {
    let pos = cursor.position() as usize;
    let data = *cursor.get_ref();
    let head = data.get(pos..pos + len).ok_or(JpegParserError::UnexpectedEndOfSegment)?;
    cursor.set_position((pos + len) as u64);
    Ok(head)
}

/// Parses a Start of Frame (SOF) segment payload.
fn parse_sof(data: &[u8], frame_header: &mut JpegFrameHeader) -> Result<(), JpegParserError> {
    // Spec B.2.2 Frame header syntax
    let mut cursor = Cursor::new(data);
    frame_header.components.clear();
    let precision = read_u8(&mut cursor)?;
    frame_header.visible_height = read_u16(&mut cursor)?;
    frame_header.visible_width = read_u16(&mut cursor)?;
    frame_header.num_components = read_u8(&mut cursor)?;

    if precision != 8 {
        return Err(JpegParserError::Only8BitPrecisionSupported);
    }
    if frame_header.num_components < 1
        || frame_header.num_components as usize > K_JPEG_MAX_COMPONENTS
    {
        return Err(JpegParserError::UnsupportedNumberOfComponents);
    }

    let mut max_h_factor = 0;
    let mut max_v_factor = 0;

    for _ in 0..frame_header.num_components {
        let id = read_u8(&mut cursor)?;
        if id > frame_header.num_components {
            return Err(JpegParserError::InvalidComponentId);
        }
        let hv = read_u8(&mut cursor)?;
        // The sampling factor is a single byte: the high 4 bits (nibble) are the
        // horizontal factor, and the low 4 bits are the vertical factor.
        let horizontal_sampling_factor = hv >> 4;
        let vertical_sampling_factor = hv & 0x0f;

        if horizontal_sampling_factor > max_h_factor {
            max_h_factor = horizontal_sampling_factor;
        }
        if vertical_sampling_factor > max_v_factor {
            max_v_factor = vertical_sampling_factor;
        }

        if !(1..=4).contains(&horizontal_sampling_factor) {
            return Err(JpegParserError::InvalidHorizontalSamplingFactor);
        }
        if !(1..=4).contains(&vertical_sampling_factor) {
            return Err(JpegParserError::InvalidVerticalSamplingFactor);
        }

        let quantization_table_selector = read_u8(&mut cursor)?;

        frame_header.components.push(JpegComponent {
            id,
            horizontal_sampling_factor,
            vertical_sampling_factor,
            quantization_table_selector,
        });
    }

    // The size of data unit is 8*8 and the coded size should be extended
    // to complete minimum coded unit, MCU. See Spec A.2.
    frame_header.coded_width =
        (frame_header.visible_width as usize).next_multiple_of(max_h_factor as usize * 8) as u16;
    frame_header.coded_height =
        (frame_header.visible_height as usize).next_multiple_of(max_v_factor as usize * 8) as u16;

    Ok(())
}

/// Parses a Define Quantization Table (DQT) segment payload.
fn parse_dqt(data: &[u8], q_table: &mut [JpegQuantizationTable]) -> Result<(), JpegParserError> {
    // Spec B.2.4.1 Quantization table-specification syntax
    let mut cursor = Cursor::new(data);
    while has_data_left(&mut cursor)? {
        let precision_and_table_id = read_u8(&mut cursor)?;
        // Precision is the upper nibble, table ID is the lower nibble.
        let precision = precision_and_table_id >> 4;
        let table_id = (precision_and_table_id & 0x0f) as usize;

        if precision > 1 {
            return Err(JpegParserError::InvalidQuantizationTablePrecision);
        }
        if precision == 1 {
            // 1 means 16-bit precision
            return Err(JpegParserError::Invalid16BitQuantizationTable);
        }
        if table_id >= K_JPEG_MAX_QUANTIZATION_TABLE_NUM {
            return Err(JpegParserError::InvalidQuantizationTableId);
        }

        let q_data = read_slice(&mut cursor, 64)?;
        q_table[table_id].valid = true;
        q_table[table_id].value.copy_from_slice(q_data);
    }
    Ok(())
}

/// Parses a Define Huffman Table (DHT) segment payload.
fn parse_dht(
    data: &[u8],
    dc_table: &mut [JpegHuffmanTable],
    ac_table: &mut [JpegHuffmanTable],
) -> Result<(), JpegParserError> {
    // Spec B.2.4.2 Huffman table-specification syntax
    let mut cursor = Cursor::new(data);
    while has_data_left(&mut cursor)? {
        let table_class_and_id = read_u8(&mut cursor)?;
        // Table class is the upper nibble, table ID is the lower nibble.
        let table_class = table_class_and_id >> 4;
        let table_id = (table_class_and_id & 0x0f) as usize;

        if table_class > 1 {
            return Err(JpegParserError::InvalidHuffmanTableClass);
        }
        if table_id >= K_JPEG_MAX_HUFFMAN_TABLE_NUM_BASELINE {
            return Err(JpegParserError::InvalidHuffmanTableId);
        }

        let code_length = read_slice(&mut cursor, K_JPEG_MAX_HUFFMAN_CODE_LENGTHS)?;
        let count = code_length.iter().copied().map(usize::from).sum::<usize>();

        if count > K_JPEG_MAX_HUFFMAN_SYMBOLS {
            return Err(JpegParserError::TooManyHuffmanCodes);
        }

        let code_value = read_slice(&mut cursor, count)?;

        let table =
            if table_class == 1 { &mut ac_table[table_id] } else { &mut dc_table[table_id] };

        table.valid = true;
        table.code_length.copy_from_slice(code_length);
        table.code_value[..count].copy_from_slice(code_value);
    }
    Ok(())
}

/// Parses a Define Restart Interval (DRI) segment payload.
fn parse_dri(data: &[u8], restart_interval: &mut u16) -> Result<(), JpegParserError> {
    // Spec B.2.4.4 Restart interval definition syntax
    if data.len() != 2 {
        return Err(JpegParserError::InvalidDriSegmentSize);
    }
    let mut cursor = Cursor::new(data);
    *restart_interval = read_u16(&mut cursor)?;
    Ok(())
}

/// Parses a Start of Scan (SOS) segment payload.
fn parse_sos(
    data: &[u8],
    frame_header: &JpegFrameHeader,
    scan: &mut JpegScanHeader,
) -> Result<(), JpegParserError> {
    // Spec B.2.3 Scan header syntax
    let mut cursor = Cursor::new(data);
    scan.num_components = read_u8(&mut cursor)?;
    if scan.num_components != frame_header.num_components {
        return Err(JpegParserError::ScanComponentCountMismatch);
    }

    for i in 0..scan.num_components as usize {
        let component_selector = read_u8(&mut cursor)?;
        let dc_and_ac_selector = read_u8(&mut cursor)?;
        // DC selector is the upper nibble, AC selector is the lower nibble.
        let dc_selector = dc_and_ac_selector >> 4;
        let ac_selector = dc_and_ac_selector & 0x0f;

        if component_selector != frame_header.components[i].id {
            return Err(JpegParserError::ComponentSelectorMismatch);
        }
        if dc_selector >= K_JPEG_MAX_HUFFMAN_TABLE_NUM_BASELINE as u8 {
            return Err(JpegParserError::InvalidDcSelector);
        }
        if ac_selector >= K_JPEG_MAX_HUFFMAN_TABLE_NUM_BASELINE as u8 {
            return Err(JpegParserError::InvalidAcSelector);
        }

        scan.components.push(JpegScanComponent { component_selector, dc_selector, ac_selector });
    }

    // Unused fields, only for value checking.
    let spectral_selection_start = read_u8(&mut cursor)?;
    let spectral_selection_end = read_u8(&mut cursor)?;
    let point_transform = read_u8(&mut cursor)?;

    if spectral_selection_start != 0 || spectral_selection_end != 63 {
        return Err(JpegParserError::UnsupportedSpectralSelection);
    }
    if point_transform != 0 {
        return Err(JpegParserError::UnsupportedPointTransform);
    }

    Ok(())
}

/// Searches for the End of Image (EOI) marker.
///
/// Returns the offset to the beginning of the EOI marker (the FF byte)
/// and the offset to the end of the image (right after the end of the
/// EOI marker) on success.
fn search_eoi(data: &[u8]) -> Result<(usize, usize), JpegParserError> {
    let mut cursor = Cursor::new(data);
    let mut eoi_begin = None;
    let mut eoi_end = None;

    while has_data_left(&mut cursor)? {
        let buf = cursor.fill_buf().map_err(|_| JpegParserError::IoError)?;

        if let Some(pos) = buf.iter().position(|&x| x == jpeg_marker::MARKER_PREFIX) {
            let marker_pos = cursor.position() as usize + pos;
            cursor.consume(pos + 1);

            let marker2 = loop {
                let byte = read_u8(&mut cursor)?;
                if byte != jpeg_marker::MARKER_PREFIX {
                    break byte;
                }
            }; // skip fill bytes

            match marker2 {
                0x00 => (),                                  // Compressed data escape.
                jpeg_marker::RST0..=jpeg_marker::RST7 => (), // Restart
                jpeg_marker::EOI => {
                    eoi_begin = Some(marker_pos);
                    eoi_end = Some(cursor.position() as usize);
                    break;
                }
                _ => {
                    // Skip for other markers.
                    let size = read_u16(&mut cursor)? as usize;
                    if size < 2 {
                        return Err(JpegParserError::InvalidMarkerSegmentSize);
                    }
                    let skip_size = size - 2;
                    let new_pos = cursor.position() as usize + skip_size;
                    let len = cursor.get_ref().len();
                    if new_pos > len {
                        return Err(JpegParserError::UnexpectedEndOfImage);
                    }
                    cursor.set_position(new_pos as u64);
                }
            }
        } else {
            let len = buf.len();
            cursor.consume(len);
        }
    }

    match (eoi_begin, eoi_end) {
        (Some(b), Some(e)) => Ok((b, e)),
        _ => Err(JpegParserError::EoiMarkerNotFound),
    }
}

impl Default for JpegHuffmanTable {
    fn default() -> Self {
        Self {
            valid: false,
            code_length: [0; K_JPEG_MAX_HUFFMAN_CODE_LENGTHS],
            code_value: [0; K_JPEG_MAX_HUFFMAN_SYMBOLS],
        }
    }
}

impl Default for JpegQuantizationTable {
    fn default() -> Self {
        Self { valid: false, value: [0; 64] }
    }
}

impl Default for JpegParseResult {
    fn default() -> Self {
        Self {
            error_code: JpegParserError::Ok,
            frame_header: Default::default(),
            dc_table: [Default::default(), Default::default()],
            ac_table: [Default::default(), Default::default()],
            q_table: [
                Default::default(),
                Default::default(),
                Default::default(),
                Default::default(),
            ],
            restart_interval: 0,
            scan: Default::default(),
            data_offset: 0,
            data_length: 0,
            image_size: 0,
        }
    }
}

/// Core JPEG parsing logic.
///
/// Iterates through JPEG segments until the Start of Scan (SOS) marker is
/// reached, then searches for the End of Image (EOI) marker.
fn parse_jpeg_picture(
    original_data: &[u8],
    res: &mut JpegParseResult,
) -> Result<(), JpegParserError> {
    // Spec B.2.1 High-level syntax
    let mut cursor = Cursor::new(original_data);

    let marker1 = read_u8(&mut cursor)?;
    let marker2 = read_u8(&mut cursor)?;

    if marker1 != jpeg_marker::MARKER_PREFIX || marker2 != jpeg_marker::SOI {
        return Err(JpegParserError::SoiMarkerNotFound);
    }

    let mut has_marker_dqt = false;
    let mut has_marker_sos = false;

    // Once reached SOS, all necessary data are parsed.
    while !has_marker_sos {
        let m1 = read_u8(&mut cursor)?;
        if m1 != jpeg_marker::MARKER_PREFIX {
            return Err(JpegParserError::ExpectedMarkerPrefix);
        }

        let m2 = loop {
            let byte = read_u8(&mut cursor)?;
            if byte != jpeg_marker::MARKER_PREFIX {
                break byte;
            }
        }; // skip fill bytes

        let size = read_u16(&mut cursor)? as usize;
        // The size includes the size field itself.
        if size < MARKER_SIZE {
            return Err(JpegParserError::InvalidMarkerSegmentSize);
        }
        let payload_size = size - 2;

        let payload = read_slice(&mut cursor, payload_size)?;

        match m2 {
            jpeg_marker::SOF0 => {
                parse_sof(payload, &mut res.frame_header)?;
            }
            jpeg_marker::SOF1
            | jpeg_marker::SOF2
            | jpeg_marker::SOF3
            | jpeg_marker::SOF5
            | jpeg_marker::SOF6
            | jpeg_marker::SOF7
            | jpeg_marker::SOF9
            | jpeg_marker::SOF10
            | jpeg_marker::SOF11
            | jpeg_marker::SOF13
            | jpeg_marker::SOF14
            | jpeg_marker::SOF15 => {
                return Err(JpegParserError::OnlySof0Supported);
            }
            jpeg_marker::DQT => {
                parse_dqt(payload, &mut res.q_table)?;
                has_marker_dqt = true;
            }
            jpeg_marker::DHT => {
                parse_dht(payload, &mut res.dc_table, &mut res.ac_table)?;
            }
            jpeg_marker::DRI => {
                parse_dri(payload, &mut res.restart_interval)?;
            }
            jpeg_marker::SOS => {
                parse_sos(payload, &res.frame_header, &mut res.scan)?;
                has_marker_sos = true;
            }
            _ => {
                // Unknown marker, skip
            }
        }
    }

    if !has_marker_dqt {
        return Err(JpegParserError::DqtMarkerNotFound);
    }

    // Scan data follows scan header immediately.
    let scan_data_start = cursor.position() as usize;

    let remaining_data = &original_data[scan_data_start..];
    let (eoi_begin_offset, eoi_end_offset) = search_eoi(remaining_data)?;

    res.data_offset = scan_data_start;
    res.data_length = eoi_begin_offset;
    res.image_size = scan_data_start + eoi_end_offset;

    Ok(())
}

/// FFI entry point for parsing a JPEG picture.
fn parse_jpeg_picture_ffi(data: &[u8]) -> JpegParseResult {
    let mut res = JpegParseResult::default();
    match parse_jpeg_picture(data, &mut res) {
        Ok(_) => {
            res.error_code = JpegParserError::Ok;
            res
        }
        Err(error_code) => JpegParseResult { error_code, ..Default::default() },
    }
}
