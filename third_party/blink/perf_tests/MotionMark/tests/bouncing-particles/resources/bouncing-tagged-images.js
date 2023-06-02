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

BouncingTaggedImage = Utilities.createSubclass(BouncingParticle,
    function(stage)
    {
        BouncingParticle.call(this, stage);

        this.element = document.createElement("img");
        this.element.style.width = this.size.x + "px";
        this.element.style.height = this.size.y + "px";
        this.element.setAttribute("src", Stage.randomElementInArray(stage.images).src);

        stage.element.appendChild(this.element);
        this._move();
    }, {

    _move: function()
    {
        this.element.style.transform = "translate(" + this.position.x + "px," + this.position.y + "px) " + this.rotater.rotateZ();
    },

    animate: function(timeDelta)
    {
        BouncingParticle.prototype.animate.call(this, timeDelta);
        this._move();
    }
});

BouncingTaggedImagesStage = Utilities.createSubclass(BouncingParticlesStage,

    function()
    {
        BouncingParticlesStage.call(this);
    }, {

    imageSrcs: [
        "image1",
        "image2",
        "image3",
        "image4",
        "image5",
    ],
    images: [],

    initialize: function(benchmark, options)
    {
        BouncingParticlesStage.prototype.initialize.call(this, benchmark, options);

        var lastPromise;
        var images = this.images;
        this.imageSrcs.forEach(function(imageSrc) {
            var promise = this._loadImage("resources/" + imageSrc + ".jpg");
            if (!lastPromise)
                lastPromise = promise;
            else {
                lastPromise = lastPromise.then(function(img) {
                    images.push(img);
                    return promise;
                });
            }
        }, this);

        lastPromise.then(function(img) {
            images.push(img);
            benchmark.readyPromise.resolve();
        });
    },

    _loadImage: function(src) {
        var img = new Image;
        var promise = new SimplePromise;

        img.onload = function(e) {
            promise.resolve(e.target);
        };

        img.src = src;
        return promise;
    },

    createParticle: function()
    {
        return new BouncingTaggedImage(this);
    },

    particleWillBeRemoved: function(particle)
    {
        particle.element.remove();
    }
});

BouncingTaggedImagesBenchmark = Utilities.createSubclass(Benchmark,
    function(options)
    {
        Benchmark.call(this, new BouncingTaggedImagesStage(), options);
    }, {

    waitUntilReady: function() {
        this.readyPromise = new SimplePromise;
        return this.readyPromise;
    }
});

window.benchmarkClass = BouncingTaggedImagesBenchmark;

})();
