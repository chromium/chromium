// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SINK_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SINK_H_

@class CollectionViewItem;
@class ContentSuggestionIdentifier;
@class ContentSuggestionsSectionInformation;
@protocol SuggestedContent;

// Data sink for the ContentSuggestions. It will be notified when new data needs
// to be pulled.
@protocol ContentSuggestionsDataSink

// Notifies the Data Sink that new data is available for the section identified
// by |sectionInfo|.
// If |forceReload| is YES, the section is reloaded with the new suggestions.
// If |forceReload| is NO, the new data are loaded only if the section is empty.
- (void)dataAvailableForSection:
            (ContentSuggestionsSectionInformation*)sectionInfo
                    forceReload:(BOOL)forceReload;

// Notifies the Data Sink that the section identified by |sectionInfo|
// |isLoading| or not.
- (void)section:(ContentSuggestionsSectionInformation*)sectionInfo
      isLoading:(BOOL)isLoading;

// The suggestion associated with |suggestionIdentifier| has been invalidated by
// the backend data source and should be cleared now. This is why this method is
// about the data source pushing something to the data sink.
- (void)clearSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier;

// Notifies the Data Sink that it must remove all current data and reload new
// ones.
- (void)reloadAllData;

// Informs the data sink to add |sectionInfo| to the model and call |completion|
// if a section is added. If the section already exists, |completion| will not
// be called.
- (void)addSection:(ContentSuggestionsSectionInformation*)sectionInfo
        completion:(void (^)(void))completion;

// The section corresponding to |sectionInfo| has been invalidated and must be
// cleared now. This is why this method is about the data source pushing
// something to the data sink.
- (void)clearSection:(ContentSuggestionsSectionInformation*)sectionInfo;

// The content of the section corresponding to |sectionInfo| must be reloaded
// now, removing the current content.
- (void)reloadSection:(ContentSuggestionsSectionInformation*)sectionInfo;

// Notifies the data sink that the |item| has changed.
- (void)itemHasChanged:(CollectionViewItem<SuggestedContent>*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SINK_H_
