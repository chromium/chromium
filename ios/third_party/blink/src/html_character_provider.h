// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_THIRD_PARTY_BLINK_SRC_HTML_CHARACTER_PROVIDER_H_
#define IOS_THIRD_PARTY_BLINK_SRC_HTML_CHARACTER_PROVIDER_H_

#include <stddef.h>

#include "ios/third_party/blink/src/html_tokenizer_adapter.h"

namespace WebCore {

const LChar kEndOfFileMarker = 0;

// CharacterProvider provides input characters to WebCore::HTMLTokenizer.
// It replaces WebCore::SegmentedString (which sits ontop of WTF::String).
class CharacterProvider {
public:
    CharacterProvider()
        : _totalBytes(0)
        , _remainingBytes(0)
        , _singleBytePtr(nullptr)
        , _doubleBytePtr(nullptr)
        , _littleEndian(false)
    {
    }

    CharacterProvider(const CharacterProvider&) = delete;
    CharacterProvider& operator=(const CharacterProvider&) = delete;

    void setContents(const LChar* str, size_t numberOfBytes)
    {
        _totalBytes = numberOfBytes;
        _remainingBytes = numberOfBytes;
        _singleBytePtr = str;
        _doubleBytePtr = nullptr;
        _littleEndian = false;
    }

    void setContents(const UChar* str, size_t numberOfBytes)
    {
        _totalBytes = numberOfBytes;
        _remainingBytes = numberOfBytes;
        _singleBytePtr = nullptr;
        _doubleBytePtr = str;
        _littleEndian = false;
    }

    void clear()
    {
        _totalBytes = 0;
        _remainingBytes = 0;
        _singleBytePtr = nullptr;
        _doubleBytePtr = nullptr;
        _littleEndian = false;
    }

    bool startsWith(const LChar* str,
                    size_t byteCount,
                    bool caseInsensitive = false) const
    {
        if (!str || byteCount > _remainingBytes)
            return false;

        for (size_t index = 0; index < byteCount; ++index) {
            UChar lhs = characterAtIndex(index);
            UChar rhs = str[index];

            if (caseInsensitive) {
                if (isASCIIUpper(lhs))
                    lhs = toLowerCase(lhs);

                if (isASCIIUpper(rhs))
                    rhs = toLowerCase(rhs);
            }

            if (lhs != rhs)
                return false;
        }

        return true;
    }

    inline UChar currentCharacter() const
    {
        return characterAtIndex(0);
    }

    inline UChar nextCharacter()
    {
        advanceBytePointer();
        return characterAtIndex(0);
    }

    inline void next()
    {
        advanceBytePointer();
    }

    inline bool isEmpty() const
    {
        return !_remainingBytes;
    }

    inline size_t remainingBytes() const
    {
        return _remainingBytes;
    }

    inline size_t bytesProvided() const
    {
        return _totalBytes - _remainingBytes;
    }

    inline void setLittleEndian()
    {
        _littleEndian = true;
    }

private:
    void advanceBytePointer()
    {
        --_remainingBytes;
        if (!_remainingBytes)
            return;

        if (_singleBytePtr)
            ++_singleBytePtr;
        else {
            DCHECK(_doubleBytePtr);
            ++_doubleBytePtr;
        }
    }

    UChar characterAtIndex(size_t index) const
    {
        if (!_remainingBytes) {
            // There is a quirk in the blink implementation wherein the empty state
            // is not set on the source until next() has been called when
            // _remainingBytes is zero. In this case, return kEndOfFileMarker.
            return kEndOfFileMarker;
        }

        ASSERT(_singleBytePtr || _doubleBytePtr);

        UChar character = kEndOfFileMarker;
        if (_singleBytePtr)
            character = _singleBytePtr[index];
        else
            character = _doubleBytePtr[index];

        if (_littleEndian)
            character = ByteSwap(character);

        return character;
    }

private:
    size_t _totalBytes;
    size_t _remainingBytes;
    const LChar* _singleBytePtr;
    const UChar* _doubleBytePtr;
    bool _littleEndian;
};

}

#endif // IOS_THIRD_PARTY_BLINK_SRC_HTML_CHARACTER_PROVIDER_H_
