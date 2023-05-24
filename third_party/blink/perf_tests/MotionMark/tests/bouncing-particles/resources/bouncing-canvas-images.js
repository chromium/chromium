/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
(function() {

BouncingCanvasImage = Utilities.createSubclass(BouncingCanvasParticle,
    function(stage)
    {
        BouncingCanvasParticle.call(this, stage, "image");
        this._imageElement = stage.imageElement;
    }, {

    _draw: function()
    {
        this.context.save();
            this.applyRotation();
            this.context.drawImage(this._imageElement, 0, 0, this.size.x, this.size.y);
        this.context.restore();
    }
});

BouncingCanvasImagesStage = Utilities.createSubclass(BouncingCanvasParticlesStage,
    function()
    {
        BouncingCanvasParticlesStage.call(this);
    }, {

    initialize: function(benchmark, options)
    {
        BouncingCanvasParticlesStage.prototype.initialize.call(this, benchmark, options);
        var imageSrc = options["imageSrc"] || "resources/yin-yang.svg";
        this.imageElement = document.querySelector(".hidden[src=\"" + imageSrc + "\"]");
    },

    createParticle: function()
    {
        return new BouncingCanvasImage(this);
    }
});

BouncingCanvasImagesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingCanvasImagesStage(), options);
    }
);

window.benchmarkClass = BouncingCanvasImagesBenchmark;

})();
