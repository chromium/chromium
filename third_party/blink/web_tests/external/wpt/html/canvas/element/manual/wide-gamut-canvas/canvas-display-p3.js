// Each PNG:
//  * is 2x2 and has a single color
//  * has a filename that indicates its contents:
//      <embedded-profile>-<8-or-16-bit-color-value>.png
//  * was generated using ImageMagick commands like:
//      convert -size 2x2 xc:'#BB0000FF' -profile Display-P3.icc Display-P3-BB0000FF.png
//      convert -size 2x2 xc:'#BBBC00000000FFFF' -profile Adobe-RGB.icc Adobe-RGB-BBBC00000000FFFF.png

// Top level key is the image filename. Second level key is the pair of
// CanvasRenderingContext2DSettings.colorSpace and ImageDataSettings.colorSpace.
const imageTests = {
    // 8 bit source images

    "sRGB-FF0000FF.png": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 0, 0, 255],
        "display-p3 display-p3": [234, 51, 35, 255],
    },
    "sRGB-FF0000CC.png": {
        "srgb srgb": [255, 0, 0, 204],
        "srgb display-p3": [234, 51, 35, 204],
        "display-p3 srgb": [255, 0, 0, 204],
        "display-p3 display-p3": [234, 51, 35, 204],
    },
    "sRGB-BB0000FF.png": {
        "srgb srgb": [187, 0, 0, 255],
        "srgb display-p3": [171, 35, 23, 255],
        "display-p3 srgb": [187, 1, 0, 255],
        "display-p3 display-p3": [171, 35, 23, 255],
    },
    "sRGB-BB0000CC.png": {
        "srgb srgb": [187, 0, 0, 204],
        "srgb display-p3": [171, 35, 23, 204],
        "display-p3 srgb": [187, 1, 0, 204],
        "display-p3 display-p3": [171, 35, 23, 204],
    },

    "Display-P3-FF0000FF.png": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 0, 0, 255],
        "display-p3 display-p3": [255, 0, 0, 255],
    },
    "Display-P3-FF0000CC.png": {
        "srgb srgb": [255, 0, 0, 204],
        "srgb display-p3": [234, 51, 35, 204],
        "display-p3 srgb": [255, 0, 0, 204],
        "display-p3 display-p3": [255, 0, 0, 204],
    },
    "Display-P3-BB0000FF.png": {
        "srgb srgb": [205, 0, 0, 255],
        "srgb display-p3": [188, 39, 26, 255],
        "display-p3 srgb": [205, 0, 0, 255],
        "display-p3 display-p3": [187, 0, 0, 255],
    },
    "Display-P3-BB0000CC.png": {
        "srgb srgb": [205, 0, 0, 204],
        "srgb display-p3": [188, 39, 26, 204],
        "display-p3 srgb": [205, 0, 0, 204],
        "display-p3 display-p3": [187, 0, 0, 204],
    },

    "Adobe-RGB-FF0000FF.png": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 19, 11, 255],
        "display-p3 display-p3": [255, 61, 43, 255],
    },
    "Adobe-RGB-FF0000CC.png": {
        "srgb srgb": [255, 0, 0, 204],
        "srgb display-p3": [234, 51, 35, 204],
        "display-p3 srgb": [255, 19, 11, 204],
        "display-p3 display-p3": [255, 61, 43, 204],
    },
    "Adobe-RGB-BB0000FF.png": {
        "srgb srgb": [219, 0, 0, 255],
        "srgb display-p3": [201, 42, 29, 255],
        "display-p3 srgb": [219, 0, 1, 255],
        "display-p3 display-p3": [201, 42, 29, 255],
    },
    "Adobe-RGB-BB0000CC.png": {
        "srgb srgb": [219, 0, 0, 204],
        "srgb display-p3": [201, 42, 29, 204],
        "display-p3 srgb": [219, 0, 1, 204],
        "display-p3 display-p3": [201, 42, 29, 204],
    },

    "Generic-CMYK-FF000000.jpg": {
        "srgb srgb": [0, 163, 218, 255],
        "srgb display-p3": [72, 161, 213, 255],
        "display-p3 srgb": [0, 163, 218, 255],
        "display-p3 display-p3": [0, 160, 213, 255],
    },
    "Generic-CMYK-BE000000.jpg": {
        "srgb srgb": [0, 180, 223, 255],
        "srgb display-p3": [80, 177, 219, 255],
        "display-p3 srgb": [0, 180, 223, 255],
        "display-p3 display-p3": [65, 177, 219, 255],
    },

    // 16 bit source images

    "sRGB-FFFF00000000FFFF.png": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 0, 0, 255],
        "display-p3 display-p3": [234, 51, 35, 255],
    },
    "sRGB-FFFF00000000CCCC.png": {
        "srgb srgb": [255, 0, 0, 204],
        "srgb display-p3": [234, 51, 35, 204],
        "display-p3 srgb": [255, 0, 0, 204],
        "display-p3 display-p3": [234, 51, 35, 204],
    },
    "sRGB-BBBC00000000FFFF.png": {
        "srgb srgb": [187, 0, 0, 255],
        "srgb display-p3": [171, 35, 23, 255],
        "display-p3 srgb": [187, 1, 0, 255],
        "display-p3 display-p3": [171, 35, 23, 255],
    },
    "sRGB-BBBC00000000CCCC.png": {
        "srgb srgb": [187, 0, 0, 204],
        "srgb display-p3": [171, 35, 23, 204],
        "display-p3 srgb": [187, 1, 0, 204],
        "display-p3 display-p3": [171, 35, 23, 204],
    },

    "Display-P3-FFFF00000000FFFF.png": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 0, 0, 255],
        "display-p3 display-p3": [255, 0, 0, 255],
    },
    "Display-P3-FFFF00000000CCCC.png": {
        "srgb srgb": [255, 0, 0, 204],
        "srgb display-p3": [234, 51, 35, 204],
        "display-p3 srgb": [255, 0, 0, 204],
        "display-p3 display-p3": [255, 0, 0, 204],
    },
    "Display-P3-BBBC00000000FFFF.png": {
        "srgb srgb": [205, 0, 0, 255],
        "srgb display-p3": [188, 39, 26, 255],
        "display-p3 srgb": [205, 0, 0, 255],
        "display-p3 display-p3": [187, 0, 0, 255],
    },
    "Display-P3-BBBC00000000CCCC.png": {
        "srgb srgb": [205, 0, 0, 204],
        "srgb display-p3": [188, 39, 26, 204],
        "display-p3 srgb": [205, 0, 0, 204],
        "display-p3 display-p3": [187, 0, 0, 204],
    },

    "Adobe-RGB-FFFF00000000FFFF.png": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 19, 11, 255],
        "display-p3 display-p3": [255, 61, 43, 255],
    },
    "Adobe-RGB-FFFF00000000CCCC.png": {
        "srgb srgb": [255, 0, 0, 204],
        "srgb display-p3": [234, 51, 35, 204],
        "display-p3 srgb": [255, 19, 11, 204],
        "display-p3 display-p3": [255, 61, 43, 204],
    },
    "Adobe-RGB-BBBC00000000FFFF.png": {
        "srgb srgb": [219, 0, 0, 255],
        "srgb display-p3": [201, 42, 29, 255],
        "display-p3 srgb": [219, 0, 1, 255],
        "display-p3 display-p3": [201, 42, 29, 255],
    },
    "Adobe-RGB-BBBC00000000CCCC.png": {
        "srgb srgb": [219, 0, 0, 204],
        "srgb display-p3": [201, 42, 29, 204],
        "display-p3 srgb": [219, 0, 1, 204],
        "display-p3 display-p3": [201, 42, 29, 204],
    },
};

const svgImageTests = {
    // SVG source images

    "sRGB-FF0000.svg": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 0, 0, 255],
        "display-p3 display-p3": [234, 51, 35, 255],
    },
    "sRGB-BB0000.svg": {
        "srgb srgb": [187, 0, 0, 255],
        "srgb display-p3": [171, 35, 23, 255],
        "display-p3 srgb": [187, 1, 0, 255],
        "display-p3 display-p3": [171, 35, 23, 255],
    },

    "Display-P3-1-0-0.svg": {
        "srgb srgb": [255, 0, 0, 255],
        "srgb display-p3": [234, 51, 35, 255],
        "display-p3 srgb": [255, 0, 0, 255],
        "display-p3 display-p3": [255, 0, 0, 255],
    },
    "Display-P3-0.7333-0-0.svg": {
        "srgb srgb": [205, 0, 0, 255],
        "srgb display-p3": [188, 39, 26, 255],
        "display-p3 srgb": [205, 0, 0, 255],
        "display-p3 display-p3": [187, 0, 0, 255],
    },
};

const fromSRGBToDisplayP3 = {
    "255,0,0,255": [234, 51, 35, 255],
    "255,0,0,204": [234, 51, 35, 204],
    "187,0,0,255": [171, 35, 23, 255],
    "187,0,0,204": [171, 35, 23, 204],
};

const fromDisplayP3ToSRGB = {
    "255,0,0,255": [255, 0, 0, 255],
    "255,0,0,204": [255, 0, 0, 204],
    "187,0,0,255": [205, 0, 0, 255],
    "187,0,0,204": [205, 0, 0, 204],
};

function pixelsApproximatelyEqual(p1, p2) {
    for (let i = 0; i < 4; ++i) {
        if (Math.abs(p1[i] - p2[i]) > 2)
            return false;
    }
    return true;
}
