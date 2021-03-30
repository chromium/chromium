# WebCodecs Test Files

[TOC]

## Instructions

To add, update or remove a test file, please update the list below.

Please provide full reference and steps to generate the test file so that
any people can regenerate or update the file in the future.

## List of Test Files

### four-colors.png
Generated using MSPaint like a true professional.

### four-colors.avif
Lossless encoding must be used to ensure colors are perfect.
```
avifenc -l four-colors.png -o four-colors.avif
```

### four-colors.webp
Lossless encoding must be used to ensure colors are perfect.
```
ffmpeg -i four-colors.png -lossless 1 -y four-colors.webp
```

### four-colors-limited-range-420-8bpc.webp
```
ffmpeg -i four-colors.png -pix_fmt yuv420p four-colors-limited-range-420-8bpc.webp
```

### four-colors.gif
High quality encoding must be used to ensure colors are perfect.
```
cp four-colors.png four-colors2.png
gifski -o four-colors.gif four-colors*.png
```

### four-colors-flip.gif
High quality encoding must be used to ensure colors are perfect.
```
ffmpeg -i four-colors.png -vf "rotate=PI" four-colors2.png
gifski -o four-colors-flip.gif four-colors*.png
```

### four-colors-flip.avif
```
ffmpeg -i four-colors-flip.gif -vcodec libaom-av1 -crf 16 four-colors-flip.mp4
mp4box -add-image ref:primary:tk=1:samp=1 -ab avis -ab avif -ab miaf -brand avis four-colors-flip.mp4 -out four-colors-flip.avif
```

### four-colors-limited-range-(420|422|444)-8bpc.avif
```
avifenc -r l -d 8 -y 420 -s 0 four-colors.png four-colors-limited-range-420-8bpc.avif
avifenc -r l -d 8 -y 422 -s 0 four-colors.png four-colors-limited-range-422-8bpc.avif
avifenc -r l -d 8 -y 444 -s 0 four-colors.png four-colors-limited-range-444-8bpc.avif
```

### four-colors-full-range-bt2020-pq-444-10bpc.avif
```
avifenc -r f -d 10 -y 444 -s 0 --nclx 9/16/9 four-colors.png four-colors-full-range-bt2020-pq-444-10bpc.avif
```

### four-colors.jpg
Used [Sqoosh.app](https://squoosh.app/) with MozJPEG compression and RGB
channels. exiftool was then used to add an orientation marker.
```
exiftool -Orientation=1 -n four-colors.jpg
```

### four-colors-limited-range-420-8bpc.jpg
Used [Sqoosh.app](https://squoosh.app/) with MozJPEG compression and YUV
channels.

### four-colors.mp4
Used a [custom tool](https://storage.googleapis.com/dalecurtis/avif2mp4.html) to convert four-colors.avif into a .mp4 file.
