#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
This script is used to generate a multitude of HLS content in order to test
the capabilities of HLS demuxers, including AndroidMediaPlayer and Chrome's
builtin demuxer. The generated files should be added to the videostack/testdata
folder in cloud storage.
'''

import dataclasses
import enum
import inspect
import os
import subprocess as sp
import sys
import tempfile
import typing


class Box():
  def __init__(self, value):
    self._value = value
  def value(self):
    return self._value
  def set(self, value):
    self._value = value


class Container(enum.Enum):
  TS = ('mpegts', 'ts')
  AAC = ('aac', 'aac')
  MP4 = ('fmp4', 'mp4')
  FLAC = ('flac', 'mp4')
  AC3 = ('ac3', 'mp4')

  def GetFlag(self):
    return ['-hls_segment_type', self.value[0]]

  def GetExtension(self):
    return self.value[1]


class H264Codec():
  def IsVideo(self):
    return True
  def GetFlag(self, name:str, id:int):
    return ['-map', f'[{name}out]', f'-c:v:{id}', 'libx264', '-x264-params',
            'nal-hrd=cbr:force-cfr=1', f'-b:v:{id}', '5M', f'-maxrate:v:{id}',
            '5M', f'-minrate:v:{id}', '5M', f'-bufsize:v:{id}', '10M',
            '-preset', 'slow', '-g', '48', '-sc_threshold', '0', '-keyint_min',
            '48']


class H265Codec():
  def IsVideo(self):
    return True
  def GetFlag(self, name:str, id:int):
    return ['-map', f'[{name}out]', f'-c:v:{id}', 'libx265']


class VP9Codec():
  def IsVideo(self):
    return True
  def GetFlag(self, name:str, id:int):
    return ['-map', f'[{name}out]', f'-c:v:{id}', 'libvpx-vp9']


class AV1Codec():
  def IsVideo(self):
    return True
  def GetFlag(self, name:str, id:int):
    return ['-map', f'[{name}out]', f'-c:v:{id}', 'libaom-av1']


class AACCodec():
  def IsVideo(self):
    return False
  def GetFlag(self, name:str, id:int):
    return ['-map', 'a:0', f'-c:a:{id}', 'aac', f'-b:a:{id}', '96k', '-ac', '2']


class FLACCodec():
  def IsVideo(self):
    return False
  def GetFlag(self, name:str, id:int):
    return ['-map', 'a:0', f'-c:a:{id}', 'flac',
           f'-b:a:{id}', '96k', '-ac', '2']


class AC3Codec():
  def IsVideo(self):
    return False
  def GetFlag(self, name:str, id:int):
    return ['-map', 'a:0', f'-c:a:{id}', 'ac3', f'-b:a:{id}', '96k']


@dataclasses.dataclass
class HlsRendition():
  codecs:list
  resolution:tuple = None
  rendition_id:int = None
  rendition_name:str = None

  def GetFlags(self):
    for codec in self.codecs:
      yield codec.GetFlag(self.Name(), self.ID())

  def IsVideo(self):
    return any(x.IsVideo() for x in self.codecs)

  def IsAudio(self):
    return any((not x.IsVideo()) for x in self.codecs)

  def ID(self):
    return self.rendition_id

  def Name(self):
    return self.rendition_name

  def GetMediaName(self):
    name = '+'.join(c.__class__.__name__[:-5] for c in self.codecs)
    if self.resolution:
      w, h = self.resolution
      name += f'.{w}x{h}'
    return f'{self.rendition_id}{name}'


class HlsStream(typing.NamedTuple):
  renditions: list[HlsRendition]
  container:Container
  is_vod: bool = True  # TODO: support live
  keyinfo: str = None

  def GetMediaDirname(self):
    name = f'vod_{self.container.value[0]}_'
    result = name + '_'.join(r.GetMediaName() for r in self.renditions)
    if self.keyinfo is not None:
      result = f'enc_{result}'
    return result

  def GetOutputFlags(self):
    extension = self.container.GetExtension()
    mediadir = self.GetMediaDirname()
    if not os.path.exists(mediadir):
      os.makedirs(mediadir)
    if len(self.renditions) > 1:
      yield ['-hls_segment_filename',
            f'{mediadir}/stream_%v/data%02d.{extension}']
      yield ['-master_pl_name', '"playlist.m3u8"']
      mediaplaylist = f'{mediadir}/stream_%v/stream.m3u8'
    else:
      yield ['-hls_segment_filename', f'{mediadir}/data%02d.{extension}']
      mediaplaylist = f'{mediadir}/playlist.m3u8'
    yield ['-var_stream_map', self.GetStreamPairs(), mediaplaylist]

  def GetStreamPairs(self):
    def _GetStreamPairs():
      for r in self.renditions:
        if r.IsAudio() and r.IsVideo():
          yield f'v:{r.ID()},a:{r.ID()}'
        elif r.IsAudio():
          yield f'a:{r.ID()}'
        elif r.IsVideo():
          yield f'v:{r.ID()}'
    return ' '.join(_GetStreamPairs())

  def GetFlags(self, input:str):
    yield ['ffmpeg', '-i', input]
    video_renditions = [r for r in self.renditions if r.IsVideo()]
    count = len(video_renditions)
    if video_renditions:
      names = ''
      copies = []
      for number, rendition in zip(range(count), video_renditions):
        name = f'v{number+1}'
        names += f'[{name}]'
        operation = 'copy'
        if rendition.resolution is not None:
          w,h = rendition.resolution
          operation = f'scale=w={w}:h={h}'
        copies.append(f'[{name}]{operation}[{name}out]')
        rendition.rendition_id = number
        rendition.rendition_name = name
      copies = ";".join(copies)
      yield ['-filter_complex', f'[0:v]split={count}{names};{copies}']
      for rendition in video_renditions:
        yield from rendition.GetFlags()

    for rendition in [r for r in self.renditions if r.IsAudio()]:
      if rendition.ID() is None:
        rendition.rendition_id = count
        count += 1
        rendition.rendition_name = 'AUDIORENDITIONSHAVENONAMES'
      yield from rendition.GetFlags()

    yield ['-f', 'hls']
    yield ['-hls_time', '1']
    yield ['-hls_playlist_type', 'vod']  # TODO: support live
    yield ['-hls_flags', 'independent_segments']

    if self.keyinfo is not None:
      # Pregenerate directory
      list(self.GetOutputFlags())
      prefix = 'https://storage.googleapis.com/videostack/testdata/hls'
      uri = f'{prefix}/{self.GetMediaDirname()}/enc.key'
      filename = os.path.join(self.GetMediaDirname(), 'enc.key')
      enc_iv = sp.check_output(['openssl', 'rand', '-hex', '16']).decode()
      enc_key = sp.check_output(['openssl', 'rand', '16'])
      with open(filename, 'wb') as f:
        f.write(enc_key)
      with open(self.keyinfo, 'w') as tmp:
        tmp.write(f'{uri}\n') # URI for key
        tmp.write(f'{filename}\n') # filepath for key
        tmp.write(enc_iv)
        tmp.flush()
      yield ['-hls_key_info_file', self.keyinfo]

    yield self.container.GetFlag()
    yield from self.GetOutputFlags()


def create_single_codecs_per_container(args):
  video = [H264Codec, H265Codec, VP9Codec]
  audio = [AACCodec, FLACCodec, AC3Codec]
  # All codecs are supported in mp4.
  for codec in video+audio:
    yield HlsStream(renditions=[HlsRendition(codecs=[codec()])],
                    container=Container.MP4)

  for codec in audio+[H264Codec, H265Codec]:
    # FFmpeg cant generate vp9/av1 in ts files.
    yield HlsStream(renditions=[HlsRendition(codecs=[codec()])],
                    container=Container.TS)


def create_enc_content(args):
  with tempfile.NamedTemporaryFile(mode='w') as tmp:
    yield HlsStream(
      renditions=[HlsRendition(codecs=[H264Codec(), AACCodec()])],
      container=Container.TS,
      keyinfo=tmp.name)


def create_different_res_ts(args):
  yield HlsStream(renditions=[HlsRendition(codecs=[H264Codec(), AACCodec()]),
                              HlsRendition(codecs=[H264Codec(), AACCodec()],
                                           resolution=(184, 328))],
                  container=Container.TS)


def _render_help():
  print('Usage:')
  print('./generate_hls.py [command]')
  print('\navailable commands:')
  for symbol in globals():
    if symbol.startswith('create_'):
      print(f'  {symbol}')


def _gen_tempfile(outfile):
  gen_video = ['-f', 'lavfi', '-i', 'testsrc2=d=6:s=1920x1080:r=24']
  gen_audio = ['-f', 'lavfi', '-i', 'sine=f=440:b=4:d=6']
  sp.check_output(['ffmpeg'] + gen_audio + gen_video + [outfile])


def _gen_media(stream, infile):
  def join(stream):
    for iterable in stream:
      yield from iterable
  sp.check_output(list(join(stream.GetFlags(infile))))


def main(args):
  if not len(args):
    return _render_help()
  if not args[0].startswith('create_'):
    return _render_help()
  if args[0] not in globals():
    return _render_help()

  with tempfile.TemporaryDirectory() as tmp:
    media_file = os.path.join(tmp, 'testsrc2.mp4')
    _gen_tempfile(media_file)
    for stream in globals()[args[0]](args[1:]):
      for flag in stream.GetFlags(media_file):
        print(' '.join(flag), '\\')
      _gen_media(stream, media_file)


if __name__ == '__main__':
  main(sys.argv[1:])
