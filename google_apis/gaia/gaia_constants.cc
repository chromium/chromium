// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants definitions

#include "google_apis/gaia/gaia_constants.h"

namespace GaiaConstants {

// Gaia uses this for accounting where login is coming from.
const char kChromeOSSource[] = "chromeos";
const char kChromeSource[] = "ChromiumBrowser";
const char kUnexpectedServiceResponse[] = "UnexpectedServiceResponse";

// OAuth scopes.
const char kOAuth1LoginScope[] = "https://www.google.com/accounts/OAuthLogin";

// Service/scope names for device management (cloud-based policy) server.
const char kDeviceManagementServiceOAuth[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";

// OAuth2 scope for access to all Google APIs.
const char kAnyApiOAuth2Scope[] = "https://www.googleapis.com/auth/any-api";

// OAuth2 scope for access to Chrome sync APIs
const char kChromeSyncOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromesync";
// OAuth2 scope for access to the Chrome Sync APIs for managed profiles.
const char kChromeSyncSupervisedOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromesync_playpen";

// OAuth2 scope for parental consent logging for secondary account addition.
const char kKidManagementPrivilegedOAuth2Scope[] =
    "https://www.googleapis.com/auth/kid.management.privileged";

// OAuth2 scope for access to Google Family Link Supervision Setup.
const char kKidsSupervisionSetupChildOAuth2Scope[] =
    "https://www.googleapis.com/auth/kids.supervision.setup.child";

// OAuth2 scope for access to Google Talk APIs (XMPP).
const char kGoogleTalkOAuth2Scope[] =
    "https://www.googleapis.com/auth/googletalk";

// OAuth2 scope for access to Google account information.
const char kGoogleUserInfoEmail[] =
    "https://www.googleapis.com/auth/userinfo.email";
const char kGoogleUserInfoProfile[] =
    "https://www.googleapis.com/auth/userinfo.profile";

// OAuth2 scope for IP protection proxy authentication
const char kIpProtectionAuthScope[] =
    "https://www.googleapis.com/auth/ip-protection";

// OAuth2 scope for access to the parent approval widget.
const char kParentApprovalOAuth2Scope[] =
    "https://www.googleapis.com/auth/kids.parentapproval";

// OAuth2 scope for access to the people API (read-only).
const char kPeopleApiReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/peopleapi.readonly";

// OAuth2 scope for access to the people API (read-write).
const char kPeopleApiReadWriteOAuth2Scope[] =
    "https://www.googleapis.com/auth/peopleapi.readwrite";

// OAuth2 scope for access to the people API person's locale preferences
// (read-only).
const char kProfileLanguageReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/profile.language.read";

// OAuth2 scope for access to the programmatic challenge API (read-only).
const char kProgrammaticChallengeOAuth2Scope[] =
    "https://www.googleapis.com/auth/accounts.programmaticchallenge";

// OAuth2 scope for access to the Reauth flow.
const char kAccountsReauthOAuth2Scope[] =
    "https://www.googleapis.com/auth/accounts.reauth";

// OAuth2 scope for access to audit recording (ARI).
const char kAuditRecordingOAuth2Scope[] =
    "https://www.googleapis.com/auth/auditrecording-pa";

// OAuth2 scope for access to clear cut logs.
const char kClearCutOAuth2Scope[] = "https://www.googleapis.com/auth/cclog";

// OAuth2 scope for FCM, the Firebase Cloud Messaging service.
const char kFCMOAuthScope[] =
    "https://www.googleapis.com/auth/firebase.messaging";

// OAuth2 scope for access to Tachyon api.
const char kTachyonOAuthScope[] = "https://www.googleapis.com/auth/tachyon";

// OAuth2 scope for access to the Photos API.
const char kPhotosOAuth2Scope[] = "https://www.googleapis.com/auth/photos";

// OAuth2 scope for access to the SecureConnect API.
extern const char kSecureConnectOAuth2Scope[] =
    "https://www.googleapis.com/auth/bce.secureconnect";

// OAuth2 scope for access to Cast backdrop API.
const char kCastBackdropOAuth2Scope[] =
    "https://www.googleapis.com/auth/cast.backdrop";

// OAuth2 scope for access to passwords leak checking API.
const char kPasswordsLeakCheckOAuth2Scope[] =
    "https://www.googleapis.com/auth/identity.passwords.leak.check";

// OAuth2 scope for access to Chrome safe browsing API.
const char kChromeSafeBrowsingOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-safe-browsing";

// OAuth2 scope for access to kid permissions by URL.
const char kClassifyUrlKidPermissionOAuth2Scope[] =
    "https://www.googleapis.com/auth/kid.permission";
const char kKidFamilyReadonlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/kid.family.readonly";

// OAuth2 scope for access to payments.
const char kPaymentsOAuth2Scope[] =
    "https://www.googleapis.com/auth/wallet.chrome";

const char kCryptAuthOAuth2Scope[] =
    "https://www.googleapis.com/auth/cryptauth";

// OAuth2 scope for access to Drive.
const char kDriveOAuth2Scope[] = "https://www.googleapis.com/auth/drive";

// OAuth2 scope for access for DriveFS to access flags.
const char kExperimentsAndConfigsOAuth2Scope[] =
    "https://www.googleapis.com/auth/experimentsandconfigs";

// The scope required for an access token in order to query ItemSuggest.
const char kDriveReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/drive.readonly";

// OAuth2 scope for access to Assistant SDK.
const char kAssistantOAuth2Scope[] =
    "https://www.googleapis.com/auth/assistant-sdk-prototype";

// OAuth2 scope for access to nearby devices (fast pair) APIs.
const char kNearbyDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/nearbydevices-pa";

// OAuth2 scope for access to nearby sharing.
const char kNearbyShareOAuth2Scope[] =
    "https://www.googleapis.com/auth/nearbysharing-pa";

// OAuth2 scope for access to nearby sharing.
const char kNearbyPresenceOAuth2Scope[] =
    "https://www.googleapis.com/auth/nearbypresence-pa";

// OAuth2 scopes for access to GCM account tracker.
const char kGCMGroupServerOAuth2Scope[] = "https://www.googleapis.com/auth/gcm";
const char kGCMCheckinServerOAuth2Scope[] =
    "https://www.googleapis.com/auth/android_checkin";

// OAuth2 scope for access to readonly Chrome web store.
const char kChromeWebstoreOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromewebstore.readonly";

// OAuth2 scope for access to Account Capabilities API.
const char kAccountCapabilitiesOAuth2Scope[] =
    "https://www.googleapis.com/auth/account.capabilities";

// OAuth2 scope for support content API.
const char kSupportContentOAuth2Scope[] =
    "https://www.googleapis.com/auth/supportcontent";

// OAuth 2 scope for NTP Photos module API.
const char kPhotosModuleOAuth2Scope[] =
    "https://www.googleapis.com/auth/photos.firstparty.readonly";

// OAuth 2 scope for NTP Photos module image API.
const char kPhotosModuleImageOAuth2Scope[] =
    "https://www.googleapis.com/auth/photos.image.readonly";

// OAuth 2 scope for the Discover feed.
const char kFeedOAuth2Scope[] = "https://www.googleapis.com/auth/googlenow";

// OAuth 2 scope for the k-Anonymity Service API.
const char kKAnonymityServiceOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromekanonymity";

// OAuth 2 scope for readonly access to Calendar.
const char kCalendarReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/calendar.readonly";

// OAuth 2 scope for Google Password Manager passkey enclaves.
const char kPasskeysEnclaveOAuth2Scope[] =
  "https://www.googleapis.com/auth/secureidentity.action";

// OAuth2 scope for Cloud Search query API.
const char kCloudSearchQueryOAuth2Scope[] =
    "https://www.googleapis.com/auth/cloud_search.query";

// OAuth 2 scopes for Google Tasks API.
const char kTasksReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/tasks.readonly";
const char kTasksOAuth2Scope[] = "https://www.googleapis.com/auth/tasks";

// OAuth 2 scope for YouTube Music API.
const char kYouTubeMusicOAuth2Scope[] = "https://www.googleapis.com/auth/music";

// OAuth 2 scopes for Google Classroom API.
const char kClassroomReadOnlyCoursesOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.courses.readonly";
const char kClassroomReadOnlyCourseWorkSelfOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.coursework.me.readonly";
const char kClassroomReadOnlyStudentSubmissionsSelfOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.student-submissions.me.readonly";
const char kClassroomReadOnlyRostersOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.rosters.readonly";
const char kClassroomProfileEmailOauth2Scope[] =
    "https://www.googleapis.com/auth/classroom.profile.emails";
const char kClassroomProfilePhotoUrlScope[] =
    "https://www.googleapis.com/auth/classroom.profile.photos";

// OAuth2 scopes for Optimization Guide.
const char kOptimizationGuideServiceGetHintsOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-optimization-guide";
const char kOptimizationGuideServiceModelExecutionOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-model-execution";

// OAuth2 scopes for Lens.
const char kLensOAuth2Scope[] = "https://www.googleapis.com/auth/lens";

// OAuth2 scope for DevTools GenAI features.
const char kAidaOAuth2Scope[] = "https://www.googleapis.com/auth/aida";

// Used to build ClientOAuth requests.  These are the names of keys used when
// building base::DictionaryValue that represent the json data that makes up
// the ClientOAuth endpoint protocol.  The comment above each constant explains
// what value is associated with that key.

// Canonical email of the account to sign in.
const char kClientOAuthEmailKey[] = "email";

// Used as an Invalid refresh token.
const char kInvalidRefreshToken[] = "invalid_refresh_token";

// Name of the Google authentication cookie.
const char kGaiaSigninCookieName[] = "SAPISID";

}  // namespace GaiaConstants
